
#include <algorithm>
#include <chrono>
#include <experimental/filesystem>
#include <fstream>
#include <iostream>
#include <regex>

#include <boost/program_options.hpp>

#include "native-2-web-connection.hpp"
#include "native-2-web-plugin.hpp"

using namespace std;
using namespace std::experimental;
using namespace boost::asio;
using namespace beast;
using namespace n2w;

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

const filesystem::path web_root = filesystem::current_path();
map<vector<string>, n2w::plugin> plugins;

void reload_plugins() {
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
}

n2w::plugin server = []() {
  n2w::plugin server;
  server.register_service(N2W__DECLARE_API(reload_plugins), "");
  return server;
}();

string create_modules() {
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
}

// Servers need to know other servers
// Servers can redirect to other servers
// n2w_connection for clients
// Status updates via multicasting
// - Startup, shutdown
// - Accept, connect, close, fault
// - Number of threads, tasks
// Load balancing
// Choose plugin combinations

int main(int c, char **v) {
  using namespace boost::program_options;
  options_description options;
  options.add_options()(
      "help",
      value<bool>()->zero_tokens()->default_value(false)->implicit_value(true),
      "Display this help message.\n")(
      "address",
      value<string>()->default_value(ip::address_v6::any().to_string()),
      "IPv4 or IPv6 address to listen for connections.\n")(
      "port", value<unsigned short>()->default_value(9001),
      "Base port number to listen for connections.\n")(
      "port-range", value<unsigned>()->default_value(1),
      "Number of ports to reserve starting from server-port.\n")(
      "worker-threads", value<unsigned>()->default_value(0),
      "Number of threads for processing requests.\n'0' for default.\n")(
      "accept-threads", value<unsigned>()->default_value(0),
      "Number of threads for listening for connections.\n'0' for default.\n")(
      "connect-threads", value<unsigned>()->default_value(0),
      "Number of threads for connecting to servers.\n'0' for default.\n")(
      "worker-sessions", value<unsigned>()->default_value(0),
      "Number of worker servers to to handle long running tasks. If "
      "unspecified or '0', do not use workers.\n")(
      "multicast-address", "IPv4 or IPv6 multicast group address for serer to "
                           "manage child processes.");

  variables_map arguments;
  store(parse_command_line(c, v, options), arguments);
  notify(arguments);

  if (arguments["help"].as<bool>()) {
    clog << options;
    return 0;
  }

  io_service service;
  io_service::work work{service};

  static function<vector<uint8_t>(const vector<uint8_t> &)> null_ref;
  struct websocket_handler {
    reference_wrapper<const function<vector<uint8_t>(const vector<uint8_t> &)>>
        service = null_ref;

    void decorate(const http::request<http::string_body> &request,
                  http::response<http::string_body> &response) {
      if (!request.count(http::field::sec_websocket_protocol))
        return;
      string range = request[http::field::sec_websocket_protocol].to_string();
      regex rx{R"(\s+)"};
      vector<string> protocols, available;
      copy(sregex_token_iterator{cbegin(range), cend(range), rx, -1}, {},
           back_inserter(protocols));
      sort(begin(protocols), end(protocols));

      auto services = accumulate(
          cbegin(plugins), cend(plugins), vector<string>{},
          [](auto &all_services, auto &plugin) {
            auto services = plugin.second.get_services();
            copy(cbegin(services), cend(services), back_inserter(all_services));
            return all_services;
          });
      sort(begin(services), end(services));
      if (protocols.size() == 1 && protocols.front() == "n2w")
        available = move(services);
      else
        set_intersection(cbegin(protocols), cend(protocols), cbegin(services),
                         cend(services), back_inserter(available));
      response.insert(http::field::sec_websocket_protocol,
                      "n2w" + accumulate(cbegin(available), cend(available),
                                         string{},
                                         [](auto &services, auto &service) {
                                           return services + ' ' + service;
                                         }));
    }

    void operator()(string message) {
      for (auto &plugin : plugins) {
        service = cref(plugin.second.get_function(message));
        if (service.get())
          return;
      }
    }
    vector<uint8_t> operator()(vector<uint8_t> message) {
      if (!service.get())
        return {};
      return service(message);
    }
  };

  struct http_handler {
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

  // struct dummy_handler {};
  // accept<dummy_handler>(service, ip::address::from_string("0.0.0.0"),
  // 9003);

  struct ws_only_handler {
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

  accept<ws_only_handler>(service, ip::address_v6::any(), 9002);

  struct http_requester {
    struct websocket_handler_type {
      void decorate(http::request<http::empty_body> &request) {
        request.insert(http::field::sec_websocket_protocol, "n2w");
      }
    };

    auto operator()(const filesystem::path &p) {
      http::request<http::string_body> req;
      req.target(p.generic_u8string());
      return req;
    }
  };
  spawn(service, [&service](yield_context yield) {
    auto server = connect<http_requester>(
        service, ip::address::from_string("127.0.0.2"), 9001);
    auto server2 = move(server);
    boost::system::error_code ec;
    http::response<http::string_body> res = server2.request("/", yield[ec]);
    clog << res << '\n';
    server2.request("modules.js", [](const auto &, auto response) {
      clog << response << '\n';
    });

    auto wsclient = upgrade(move(server2));
    auto wsclient2 = wsconnect<http_requester>(
        service, ip::address::from_string("127.0.0.3"), 9001);
  });

  auto num_threads = thread::hardware_concurrency();
  clog << "Hardware concurrency: " << num_threads << '\n';
  vector<thread> threadpool;
  generate_n(back_inserter(threadpool), num_threads ? num_threads : 5,
             [&service]() { return thread{[&service]() { service.run(); }}; });
  service.run();

  return 0;
}