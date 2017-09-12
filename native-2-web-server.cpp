
#include <algorithm>
#include <chrono>
#include <experimental/filesystem>
#include <fstream>
#include <iostream>
#include <regex>

#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/program_options.hpp>

#include "native-2-web-connection.hpp"
#include "native-2-web-plugin.hpp"

using namespace std;
using namespace std::experimental;

struct normalized_uri {
  filesystem::path path;
  string query;
  string fragment;

  normalized_uri(string uri) {
    regex fragment_rx{R"((.*?)#(.*))"};
    regex query_rx{R"((.*?)\?(.*))"};
    regex scheme_rx{R"((.*?)://(.*?)/(.*))"};
    smatch match;
    string scheme;
    string host;
    if (regex_match(uri, match, scheme_rx)) {
      uri = match[3];
      scheme = match[1];
      host = match[2];
    }
    if (regex_match(uri, match, fragment_rx)) {
      fragment = match[2];
      query = match[1];
    }
    if (regex_match(query.empty() ? uri : query, match, query_rx)) {
      query = match[2];
      uri = match[1];
    }

    path = uri;
    if (path.has_relative_path())
      path = path.relative_path();
    auto segments = accumulate(
        find_if_not(begin(path), end(path),
                    [](const auto &node) { return node == ".."; }),
        end(path), vector<filesystem::path>{}, [](auto &v, const auto &p) {
          if (p != ".") {
            if (p != "..") {
              v.push_back(p);
            } else if (!v.empty()) {
              v.pop_back();
            }
          }

          return v;
        });
    auto path = accumulate(begin(segments), end(segments), filesystem::path{},
                           divides<>{});
  }

  normalized_uri(const char *uri) : normalized_uri(string(uri)) {}
};

struct server_options {
  optional<bool> spawned = false;
  optional<string> address = boost::asio::ip::address_v6::any().to_string();
  optional<unsigned short> port = 9001;
  optional<unsigned short> port_range = 1;
  optional<unsigned> worker_threads = 0;
  optional<unsigned> accept_threads = 0;
  optional<unsigned> connect_threads = 0;
  optional<unsigned> worker_sessions = 0;
  optional<string> multicast_address = "233.252.18.0";
  optional<unsigned short> multicast_port = 9002;
};

N2W__READ_WRITE_SPEC(server_options,
                     N2W__MEMBERS(address, port, port_range, worker_threads,
                                  accept_threads, connect_threads,
                                  worker_sessions, multicast_address,
                                  multicast_port));
N2W__JS_SPEC(server_options,
             N2W__MEMBERS(address, port, port_range, worker_threads,
                          accept_threads, connect_threads, worker_sessions,
                          multicast_address, multicast_port));

// Servers can redirect to other servers
// Load balancing
// Choose plugin combinations

static constexpr unsigned ring_size = 10;
struct server_statistics {
  using time_point = chrono::system_clock::time_point;
  using rep = double;
  atomic_bool spawned = false;
  atomic<rep> startup = 0, shutdown = 0;
  atomic_int32_t threads = 0, tasks = 0, connections = 0, upgrades = 0;
  array<rep, ring_size> accept = {0}, connect = {0}, upgrade = {0}, close = {0};
  array<boost::system::error_code, ring_size> error = {
      make_error_code(boost::system::errc::success)};
  atomic_uint accept_head = 0, connect_head = 0, upgrade_head = 0,
              close_head = 0, error_head = 0;

  server_statistics() = default;
  server_statistics(const server_statistics &other) { (*this) = other; }
  server_statistics &operator=(const server_statistics &other) {
    startup = other.startup.load();
    shutdown = other.shutdown.load();
    threads = other.threads.load();
    tasks = other.tasks.load();
    connections = other.connections.load();
    upgrades = other.upgrades.load();
    accept_head = other.accept_head.load();
    connect_head = other.connect_head.load();
    upgrade_head = other.upgrade_head.load();
    close_head = other.close_head.load();
    error_head = other.error_head.load();
    accept = other.accept;
    connect = other.connect;
    upgrade = other.upgrade;
    close = other.close;
    error = other.error;

    return *this;
  }

  void on_time(atomic<rep> &var) {
    var = chrono::system_clock::now().time_since_epoch().count();
  }
  void on_time_array(array<rep, ring_size> &var, atomic_uint &idx, rep rep) {
    var[idx++ % ring_size] = rep;
  }

  void on_startup() { on_time(startup); }
  void on_shutdown() { on_time(shutdown); }

  void on_thread_start() { ++threads; }
  void on_thread_end() { --threads; }
  void on_task_start() { ++tasks; }
  void on_task_end() { --tasks; }

  void on_accept(time_point t) {
    ++connections;
    on_time_array(accept, accept_head, t.time_since_epoch().count());
  }
  void on_connect(time_point t) {
    ++connections;
    on_time_array(connect, connect_head, t.time_since_epoch().count());
  }
  void on_upgrade(time_point t) {
    ++upgrades;
    on_time_array(upgrade, upgrade_head, t.time_since_epoch().count());
  }
  void on_close(time_point t, bool upgraded) {
    on_time_array(close, close_head, t.time_since_epoch().count());
    --connections;
    if (upgraded)
      --upgrades;
  }

  void on_error(boost::system::error_code &ec) {
    error[error_head++ % ring_size] = ec;
  }
};

ostream &operator<<(ostream &out, const server_statistics &stats) {
  out << stats.threads << ' ' << stats.tasks << ' ' << stats.connections << ' '
      << stats.upgrades << '\n';
  const auto print_array = [&out](const auto &array, auto index) {
    const auto offset = index % ring_size;
    copy(cbegin(array) + offset, cbegin(array) + ring_size,
         ostream_iterator<server_statistics::rep>(out, ", "));
    copy(cbegin(array), cbegin(array) + offset,
         ostream_iterator<server_statistics::rep>(out, ", "));

  };

  boost::system::error_code error[ring_size];

  print_array(stats.accept, stats.accept_head.load());
  out << '\n';
  print_array(stats.connect, stats.connect_head.load());
  out << '\n';
  print_array(stats.upgrade, stats.upgrade_head.load());
  out << '\n';
  print_array(stats.close, stats.close_head.load());
  out << '\n';

  return out;
}

N2W__BINARY_SPEC(server_statistics,
                 N2W__MEMBERS(startup, threads, tasks, connections, upgrades,
                              accept, connect, upgrade, close));
N2W__JS_SPEC(server_statistics,
             N2W__MEMBERS(startup, threads, tasks, connections, upgrades,
                          accept, connect, upgrade, close));

int main(int c, char **v) {
  using namespace boost::asio;
  using namespace beast;
  using namespace n2w;

  static const filesystem::path web_root = filesystem::current_path();
  static map<vector<string>, n2w::plugin> plugins;

  static auto reload_plugins = []() {
    clog << "Reloading plugins\n";
    const regex lib_rx{"libn2w-.+"};
    plugins.clear();
    filesystem::recursive_directory_iterator it{
        web_root, filesystem::directory_options::skip_permission_denied};
    for (const auto &entry : it) {
      auto path = entry.path();
      auto name = path.filename().generic_u8string();
      if (!regex_match(name, lib_rx)) {
        it.disable_recursion_pending();
        continue;
      }
      if (filesystem::is_directory(path))
        continue;
      if (path.extension() != ".so")
        continue;
      auto modfile = path.generic_u8string();
      path.replace_extension("");
      vector<string> hierarchy;
      auto first = begin(path);
      advance(first, distance(cbegin(web_root), cend(web_root)));
      transform(first, end(path), back_inserter(hierarchy),
                mem_fn(&filesystem::path::generic_u8string));
      transform(
          cbegin(hierarchy), cend(hierarchy),
          begin(hierarchy), [offset = strlen("libn2w-")](const auto &module) {
            return module.substr(offset);
          });
      plugins.emplace(hierarchy, modfile.c_str());
    }
    clog << "Plugins reloaded\n";
  };

  static auto spawn_server = [](optional<server_options> options) {
    clog << "Spawn server\n";
    server_options default_options;
    return system(
        ("(cd " + web_root.u8string() +
         " && ./n2w-server --spawned --address " +
         options->address.value_or(*default_options.address) + " --port " +
         to_string(options->port.value_or(*default_options.port)) +
         " --port-range " +
         to_string(options->port_range.value_or(*default_options.port_range)) +
         " --worker-threads " + to_string(options->worker_threads.value_or(
                                    *default_options.worker_threads)) +
         " --accept-threads " + to_string(options->accept_threads.value_or(
                                    *default_options.accept_threads)) +
         " --connect-threads " + to_string(options->connect_threads.value_or(
                                     *default_options.connect_threads)) +
         " --worker-sessions " + to_string(options->worker_sessions.value_or(
                                     *default_options.worker_sessions)) +
         " --multicast-address " +
         options->multicast_address.value_or(
             *default_options.multicast_address) +
         " --multicast-port " + to_string(options->multicast_port.value_or(
                                    *default_options.multicast_port)) +
         " &)&")
            .c_str());
  };

  static n2w::plugin server;
  server.register_service(N2W__DECLARE_API(reload_plugins), "");
  server.register_service("spawn_server_default_options",
                          []() { return server_options{}; }, "");
  server.register_service(N2W__DECLARE_API(spawn_server), "");
  server.register_service("stop_server", []() { raise(SIGTERM); }, "");

  static auto create_modules = []() {
    clog << "Creating modules\n";
    reload_plugins();
    string modules = "var n2w = (function () {\nlet n2w = {};\n";
    modules += "n2w['$server'] = {};\n";
    for (auto &s : server.get_services()) {
      modules += "n2w.$server." + server.get_name(s) + " = " +
                 server.get_javascript(s) + ";\n";
      modules += "n2w.$server." + server.get_name(s) +
                 ".html = " + server.get_generator(s) + ";\n";
    }

    for (auto &p : plugins) {
      string module;
      for (auto first = cbegin(p.first), last = min(first + 1, cend(p.first));
           last != cend(p.first); ++last) {
        module =
            accumulate(first, last, string{}, [](auto mods, const auto &mod) {
              return mods + "['" + mod + "']";
            });
        modules += "n2w" + module + " = n2w" + module + " || {};\n";
      }
      module = accumulate(
          cbegin(p.first), cend(p.first), string{},
          [](auto mods, const auto &mod) { return mods + "['" + mod + "']"; });
      modules += "n2w" + module + " = n2w" + module + " || {};\n";

      for (auto &s : p.second.get_services()) {
        modules += "n2w" + module + '.' + p.second.get_name(s) + " = " +
                   p.second.get_javascript(s) + ";\n";
        modules += "n2w" + module + '.' + p.second.get_name(s) +
                   ".html = " + p.second.get_generator(s) + ";\n";
      }
    }
    modules += "return n2w;\n}());\n";
    return modules;
  };

  server_options default_options;
  using namespace boost::program_options;
  options_description options;
  options.add_options()(
      "help",
      value<bool>()->zero_tokens()->default_value(false)->implicit_value(true),
      "Display this help message.\n")(
      "spawned",
      value<bool>()->zero_tokens()->default_value(false)->implicit_value(true),
      "Start server in spawned mode.\n")(
      "address", value<string>()->default_value(*default_options.address),
      "IPv4 or IPv6 address to listen for connections.\n")(
      "port", value<unsigned short>()->default_value(*default_options.port),
      "Base port number to listen for connections.\n")(
      "port-range",
      value<unsigned short>()->default_value(*default_options.port_range),
      "Number of ports to reserve starting from server-port.\n")(
      "worker-threads",
      value<unsigned>()->default_value(*default_options.worker_threads),
      "Number of threads for processing requests.\n'0' for automatic.\n")(
      "accept-threads",
      value<unsigned>()->default_value(*default_options.accept_threads),
      "Number of threads for listening for connections.\n'0' for automatic.\n")(
      "connect-threads",
      value<unsigned>()->default_value(*default_options.connect_threads),
      "Number of threads for connecting to servers.\n'0' for automatic.\n")(
      "worker-sessions",
      value<unsigned>()->default_value(*default_options.worker_sessions),
      "Number of worker servers to to handle long running tasks. If "
      "unspecified or '0', do not use workers.\n")(
      "multicast-address",
      value<string>()->default_value(*default_options.multicast_address),
      "IPv4 or IPv6 multicast group address for server to manage child "
      "processes.")(
      "multicast-port",
      value<unsigned short>()->default_value(*default_options.port + 1),
      "IPv4 or IPv6 multicast group address for server to manage "
      "child processes.");

  static variables_map arguments;
  store(parse_command_line(c, v, options), arguments);
  notify(arguments);

  if (arguments["help"].as<bool>()) {
    clog << options;
    return 0;
  }

  static io_service service;
  io_service::work work{service};
  boost::system::error_code ec;

  signal_set signals{service, SIGINT, SIGTERM};
  signals.async_wait([](const auto &ec, auto sig) { service.stop(); });

  static ip::udp::endpoint stats_endpoint(
      ip::address::from_string(arguments["multicast-address"].as<string>()),
      arguments["multicast-port"].as<unsigned short>());
  static ip::udp::socket stats_socket{service};
  stats_socket.open(stats_endpoint.protocol(), ec);
  if (ec)
    clog << ec.message() << '\n';
  stats_socket.set_option(socket_base::reuse_address{true}, ec);
  if (ec)
    clog << ec.message() << '\n';
  stats_socket.set_option(ip::multicast::enable_loopback{true}, ec);
  if (ec)
    clog << ec.message() << '\n';
  stats_socket.set_option(ip::multicast::hops{1}, ec);
  if (ec)
    clog << ec.message() << '\n';
  stats_socket.bind(
      ip::udp::endpoint(ip::address_v4::any(),
                        arguments["multicast-port"].as<unsigned short>()),
      ec);
  if (ec)
    clog << ec.message() << '\n';
  stats_socket.set_option(ip::multicast::join_group{ip::address::from_string(
                              arguments["multicast-address"].as<string>())},
                          ec);
  if (ec)
    clog << ec.message() << '\n';

  static server_statistics stats;

  spawn(service,
        [](yield_context yield) {
          const auto port = arguments["port"].as<unsigned short>();
          unsigned char buf[sizeof(stats)];
          array<const_buffer, 2> bufs{
              buffer(reinterpret_cast<const char *>(&port), sizeof(port)),
              buffer(buf, sizeof(buf))};
          boost::system::error_code ec;
          steady_timer timer{service};
          while (true) {
            timer.expires_from_now(chrono::seconds{1});
            timer.async_wait(yield[ec]);
            serialize(stats, buf);
            stats_socket.async_send_to(bufs, stats_endpoint, yield[ec]);
          }
        },
        boost::coroutines::attributes{8 << 10});

  static map<pair<string, unsigned short>, server_statistics> known_servers;
  server.register_service("known_servers", []() { return known_servers; }, "");

  spawn(service,
        [](yield_context yield) {
          unsigned short port;
          server_statistics other_stat;
          unsigned char buf[sizeof(other_stat)];
          array<mutable_buffer, 2> bufs{
              buffer(reinterpret_cast<char *>(&port), sizeof(port)),
              buffer(buf, sizeof(buf))};
          boost::system::error_code ec;
          ip::udp::endpoint endpoint;
          while (true) {
            stats_socket.async_receive_from(bufs, endpoint, yield[ec]);
            deserialize(buf, other_stat);
            known_servers[make_pair(endpoint.address().to_string(), port)] =
                other_stat;
          }
        },
        boost::coroutines::attributes{8 << 10});

  static function<vector<uint8_t>(const vector<uint8_t> &)> null_ref;
  struct websocket_handler {
    reference_wrapper<const function<vector<uint8_t>(const vector<uint8_t> &)>>
        service = null_ref;

    void decorate(const http::request<http::string_body> &request,
                  http::response<http::string_body> &response) {
      if (!request.count(http::field::sec_websocket_protocol))
        return;
      response.set(http::field::sec_websocket_protocol, "n2w");

      auto services = accumulate(
          cbegin(plugins), cend(plugins), vector<string>{},
          [](auto &all_services, auto &plugin) {
            auto services = plugin.second.get_services();
            copy(cbegin(services), cend(services), back_inserter(all_services));
            return all_services;
          });
      auto server_apis = server.get_services();
      move(begin(server_apis), end(server_apis), back_inserter(services));

      response.set(
          "X-n2w-api-list",
          accumulate(cbegin(services), cend(services),
                     string{}, [specials = regex{"\\\\|\"|\r"}](auto &services,
                                                                auto &service) {
                       return services + (services.empty() ? "" : " ") + '"' +
                              regex_replace(service, specials, "\\$0") + '"';
                     }));
    }

    void operator()(string message) {
      for (auto &plugin : plugins) {
        service = cref(plugin.second.get_function(message));
        if (service.get())
          return;
      }
      service = server.get_function(message);
    }
    vector<uint8_t> operator()(vector<uint8_t> message) {
      if (!service.get())
        return {};
      return service(message);
    }
  };

  struct stats_reporter {
    void report_task_start(server_statistics::time_point t) {
      stats.on_task_start();
    }
    void report_task_end(server_statistics::time_point t) {
      stats.on_task_end();
    }
    void report_accept(server_statistics::time_point t) { stats.on_accept(t); }
    void report_connect(server_statistics::time_point t) {
      stats.on_connect(t);
    }
    void report_upgrade(server_statistics::time_point t) {
      stats.on_upgrade(t);
    }
    void report_close(server_statistics::time_point t, bool upgraded) {
      stats.on_close(t, upgraded);
    }
    void report_error(boost::system::error_code error) {
      stats.on_error(error);
    }
  };

  struct http_handler : public stats_reporter {
    using websocket_handler_type = websocket_handler;

    http::response<http::string_body>
    operator()(const http::request<http::string_body> &request) {
      auto target = request.target();
      normalized_uri uri{string{begin(target), end(target)}};
      auto path = web_root / uri.path;
      clog << "Thread: " << this_thread::get_id()
           << "; Root directory: " << web_root << ", URI: " << target
           << ", Path: " << path << '\n';
      std::error_code ec;
      if (equivalent(path, web_root, ec))
        path /= "test.html";

      http::response<http::string_body> response;
      response.version = request.version;
      response.result(200);
      response.reason("OK");

      if (path == web_root / "modules.js") {
        response.set(http::field::content_type, "text/javascript");
        response.body = create_modules();
        cerr << response.body;
      } else if (!filesystem::exists(path, ec)) {
        response.result(404);
        response.reason("Not Found");
        response.body = ec.message();
      } else if (!filesystem::is_regular_file(path, ec)) {
        response.result(403);
        response.reason("Forbidden");
        response.body = ec ? ec.message() : "Path is not a regular file";
      } else {
        auto extension = path.extension();
        if (extension != ".html" && extension != ".htm" && extension != ".js" &&
            extension != ".css") {
          response.result(406);
          response.reason("Not Acceptable");
          response.body = "Only htm[l], javascript and cascasding style sheet "
                          "files acceptable";
        } else {
          ifstream requested{path};
          requested >> noskipws;
          copy(istream_iterator<char>(requested), {},
               back_inserter(response.body));
          string type = "text/";
          if (extension == ".html" || extension == ".htm")
            type += "html";
          else if (extension == ".css")
            type += "css";
          else if (extension == ".js")
            type += "javascript";
          else
            type += "plain";
          response.set(http::field::content_type, type);
        }
      }

      return response;
    }
  };

  accept<http_handler>(
      service, ip::address::from_string(arguments["address"].as<string>()),
      arguments["port"].as<unsigned short>());

  struct dummy_handler {};
  accept<dummy_handler>(
      service, ip::address::from_string(arguments["address"].as<string>()),
      9003);

  struct ws_only_handler : public stats_reporter {
    struct websocket_handler_type {
      function<void(string)> string_pusher;
      void operator()(function<void(string)> &&pusher) {
        string_pusher = move(pusher);
        thread t([ this, i = 0 ]() mutable {
          auto pusher = string_pusher;
          while (pusher) {
            pusher(to_string(i++) + ' ' +
                   to_string(
                       chrono::system_clock::now().time_since_epoch().count()));
            this_thread::sleep_for(chrono::milliseconds{500});
            pusher = string_pusher;
          }
        });
        t.detach();
      }
    };
  };

  accept<ws_only_handler>(
      service, ip::address::from_string(arguments["address"].as<string>()),
      9002);

  struct http_requester : public stats_reporter {
    struct websocket_handler_type {
      void decorate(http::request<http::empty_body> &request) {
        request.insert(http::field::sec_websocket_protocol, "n2w");
      }

      void inspect(http::response<http::string_body> &response) {
        clog << "Inspecting upgrade response\n";
        regex rx{R"(^"|" "|"$)"};
        auto api_list = response["X-n2w-api-list"];
        for_each(
            cregex_token_iterator{api_list.begin(), api_list.end(), rx, -1},
            {}, [rx = regex{"\\\\(.)"}](const auto &api) {
              clog << regex_replace(api.str(), rx, "$1") << '\n';
            });
      }
    };

    auto operator()(const filesystem::path &p) {
      http::request<http::string_body> req;
      req.target(p.generic_u8string());
      return req;
    }
  };
  spawn(service, [](yield_context yield) {
    auto server = connect<http_requester>(
        service, ip::address::from_string(arguments["address"].as<string>()),
        arguments["port"].as<unsigned short>());
    auto server2 = move(server);
    boost::system::error_code ec;
    http::response<http::string_body> res = server2.request("/", yield[ec]);
    clog << res << '\n';
    server2.request("modules.js", [](const auto &, auto response) {
      clog << response << '\n';
    });

    auto wsclient = upgrade(move(server2));
    auto wsclient2 = wsconnect<http_requester>(
        service, ip::address::from_string(arguments["address"].as<string>()),
        arguments["port"].as<unsigned short>());
  });

  stats.on_startup();
  auto num_threads = thread::hardware_concurrency();
  clog << "Hardware concurrency: " << num_threads << '\n';
  vector<thread> threadpool;
  generate_n(back_inserter(threadpool), num_threads ? num_threads : 5, []() {
    return thread{[]() {
      stats.on_thread_start();
      service.run();
      stats.on_thread_end();
    }};
  });
  service.run();
  stats.on_shutdown();

  return 0;
}