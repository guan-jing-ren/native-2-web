#define BOOST_ERROR_CODE_HEADER_ONLY
#include <boost/system/error_code.hpp>
inline bool operator==(boost::system::error_code ec, int i) { return ec == i; }
inline bool operator!=(boost::system::error_code ec, int i) { return ec != i; }

#include <beast/http.hpp>
#include <beast/websocket.hpp>
#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>
#define BOOST_COROUTINES_UNIDIRECT
#include <boost/asio/spawn.hpp>

#include <algorithm>
#include <chrono>
#include <experimental/filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <memory>
#include <queue>
#include <regex>

#include "native-2-web-plugin.hpp"

using namespace std;
using namespace std::experimental;
using namespace boost::asio;
using namespace beast;

template <typename Handler>
class n2w_connection : public enable_shared_from_this<n2w_connection<Handler>> {

  /***************************/
  /* HTTP SFINAE DEFINITIONS */
  /***************************/

  static auto http_support(...) -> false_type;
  template <typename T = Handler>
  static auto http_support(T *)
      -> result_of_t<T(http::request<http::string_body>)>;
  static constexpr bool supports_http =
      !is_same_v<decltype(http_support(declval<Handler *>())), false_type>;

  /**************************************/
  /* BASIC WEBSOCKET SFINAE DEFINITIONS */
  /**************************************/

  static auto websocket_support(...) -> false_type;
  template <typename T = Handler>
  static auto websocket_support(T *) -> typename T::websocket_handler_type;
  using websocket_handler_type =
      decltype(websocket_support(declval<Handler *>()));
  static constexpr bool supports_websocket =
      !is_same_v<websocket_handler_type, false_type>;

  template <typename> static auto websocket_handles(...) -> false_type;
  template <typename V, typename T = Handler>
  static auto websocket_handles(T *)
      -> result_of_t<typename T::websocket_handler_type(V)>;
  static constexpr bool websocket_handles_text =
      supports_websocket &&
      !is_same_v<decltype(websocket_handles<string>(declval<Handler *>())),
                 false_type>;
  static constexpr bool websocket_handles_binary =
      supports_websocket &&
      !is_same_v<decltype(
                     websocket_handles<vector<uint8_t>>(declval<Handler *>())),
                 false_type>;

  /*****************************************/
  /* EXTENDED WEBSOCKET SFINAE DEFINITIONS */
  /*****************************************/

  template <typename> static auto websocket_pushes(...) -> false_type;
  template <typename V, typename T = Handler>
  static auto websocket_pushes(T *)
      -> result_of_t<typename T::websocket_handler_type(function<void(V)>)>;
  static constexpr bool websocket_pushes_text =
      supports_websocket &&
      !is_same_v<decltype(websocket_pushes<string>(declval<Handler *>())),
                 false_type>;
  static constexpr bool websocket_pushes_binary =
      supports_websocket &&
      !is_same_v<decltype(
                     websocket_pushes<vector<uint8_t>>(declval<Handler *>())),
                 false_type>;

  /**********************************/
  /* INTERNAL STRUCTURE DEFINITIONS */
  /**********************************/

  struct NullHandler {
    http::response<http::string_body>
    operator()(const http::request<http::string_body> &request) {
      auto response = http::response<http::string_body>{};
      response.version = request.version;
      response.result(501);
      response.reason("Not Implemented");
      response.body = "n2w server does implement websocket handling";
      return response;
    }
  };

  struct websocket_stuff {
    websocket_handler_type websocket_handler;
    istream stream;
    websocket_stuff(boost::asio::streambuf *buf) : stream(buf) {}
  };

  /*******************/
  /* INTERNAL LAYOUT */
  /*******************/

  ip::tcp::socket socket;
  websocket::stream<ip::tcp::socket &> ws;
  atomic_uintmax_t ticket{0};
  atomic_uintmax_t serving{0};

  boost::asio::streambuf buf;
  conditional_t<supports_http, Handler, NullHandler> handler;
  conditional_t<supports_websocket, websocket_stuff, void *> ws_stuff{&buf};

  /***********************/
  /* INTERNAL OPERATIONS */
  /***********************/

  template <typename R>
  void write_response(yield_context yield, R reply, uintmax_t tkt) {
    boost::system::error_code ec;
    while (tkt != serving) {
      clog << "Waiting to serve: " << tkt << ", currently serving: " << serving
           << ", is open: " << boolalpha << socket.is_open() << '\n';
      socket.get_io_service().post(yield[ec]);
    }

    string response_type;
    if
      constexpr(is_same_v<R, http::response<http::string_body>>) {
        reply.prepare_payload();
        http::async_write(socket, reply, yield[ec]);
        response_type = "HTTP";
      }
    else if
      constexpr(!is_same_v<R, nullptr_t> && supports_websocket) {
        if
          constexpr(is_same_v<R, string>) {
            ws.text(true);
            response_type = "text websocket";
          }
        else if
          constexpr(is_same_v<R, vector<uint8_t>>) {
            ws.binary(true);
            response_type = "binary_websocket";
          }
        ws.async_write(buffer(reply), yield[ec]);
        if (ec)
          ws_stuff.websocket_handler = websocket_handler_type{};
      }

    clog << "Thread: " << this_thread::get_id() << "; Written " << response_type
         << " response:" << ec << ".\n";
    ++serving;
  }

  template <typename T> auto async(T t) {
    if
      constexpr(is_same<T, http::response<http::string_body>>::value ||
                is_same<T, string>::value ||
                is_same<T, vector<uint8_t>>::value) {
        spawn(socket.get_io_service(), [
          this, self = this->shared_from_this(), t = forward<T>(t),
          tkt = ticket++
        ](yield_context yield) { write_response(yield, move(t), tkt); });
      }
    else {
      spawn(socket.get_io_service(), [
        this, self = this->shared_from_this(), t = forward<T>(t), tkt = ticket++
      ](yield_context yield) {
        if
          constexpr(is_void<result_of_t<T()>>::value) {
            t();
            write_response(yield, nullptr, tkt);
          }
        else
          write_response(yield, t(), tkt);
      });
    }
  }

  void serve(yield_context yield) {
    boost::system::error_code ec;
    while (true) {
      http::request<http::string_body> request;
      http::async_read(socket, buf, request, yield[ec]);
      clog << "Thread: " << this_thread::get_id()
           << "; Received request: " << ec << ".\n";
      if (ec)
        break;

      if (websocket::is_upgrade(request)) {
        if
          constexpr(supports_websocket) {
            ws.async_accept(request, yield[ec]);
            clog << "Accepted websocket connection: " << ec << '\n';
            if (ec)
              break;
            ws_stuff.stream >> noskipws;
            register_websocket_pusher();
            goto do_upgrade;
          }
        else
          async(NullHandler{}(request));
        return;
      }

      async([ this, request = move(request) ]() { return handler(request); });
    }
    clog << "Finished serving socket\n";

    return;
  do_upgrade:
    ws_serve(ec, yield);
    clog << "Finished serving websocket\n";
  }

  void ws_serve(boost::system::error_code ec, yield_context yield) {
    if
      constexpr(supports_websocket) {
        auto save_stream = [this](auto &message) {
          copy(istream_iterator<char>(ws_stuff.stream), {},
               back_inserter(message));
          ws_stuff.stream.clear();
        };

        while (true) {
          ws.async_read(buf, yield[ec]);
          clog << "Read something from websocket: " << ec << '\n';
          if (ec)
            break;
          if (ws.got_text()) {
            string message;
            save_stream(message);
            clog << "Text message received: " << message << '\n';
            if
              constexpr(websocket_handles_text)
                  async([ this, message = move(message) ] {
                    return ws_stuff.websocket_handler(message);
                  });

          } else if (ws.got_binary()) {
            vector<uint8_t> message;
            save_stream(message);
            clog << "Binary message received, size: " << message.size() << '\n';
            if
              constexpr(websocket_handles_binary)
                  async([ this, message = move(message) ] {
                    return ws_stuff.websocket_handler(message);
                  });
          } else {
            clog << "Unsupported WebSocket opcode: " << '\n';
            break;
          }
        }
      }
  }

  auto register_websocket_pusher() {
    auto pusher = [ this, self = this->shared_from_this() ](auto message) {
      async(message);
    };

    if
      constexpr(websocket_pushes_text) ws_stuff.websocket_handler(pusher);
    if
      constexpr(websocket_pushes_binary) ws_stuff.websocket_handler(pusher);
  }

  /***********************/
  /* EXTERNAL INTERFACES */
  /***********************/

  n2w_connection(io_service &service)
      : socket{service}, ws{socket}, ws_stuff(&buf) {}

  template <typename, typename... Args>
  friend void accept(reference_wrapper<io_service> service, Args &&... args);
};

template <typename Handler, typename... Args>
void accept(reference_wrapper<io_service> service, Args &&... args) {
  spawn(service.get(), [ service, endpoint = ip::tcp::endpoint(args...) ](
                           yield_context yield) {
    boost::system::error_code ec;
    ip::tcp::acceptor acceptor{service, endpoint, true};
    acceptor.listen(socket_base::max_connections, ec);
    clog << "Thread: " << this_thread::get_id()
         << "; Start listening for connections: " << ec << '\n';
    if (ec)
      return;
    while (true) {
      shared_ptr<n2w_connection<Handler>> connection{
          new n2w_connection<Handler>{service}};
      acceptor.async_accept(connection->socket, yield[ec]);
      clog << "Thread: " << this_thread::get_id()
           << "; Accepted connection: " << ec << '\n';
      if (ec)
        break;
      spawn(
          service.get(), [connection = move(connection)](yield_context yield) {
            connection->serve(yield);
          });
    }
  });
}

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
  server.register_service(DECLARE_API(reload_plugins), "");
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

int main() {
  io_service service;
  io_service::work work{service};

  static function<vector<uint8_t>(const vector<uint8_t> &)> null_ref;
  struct websocket_handler {
    reference_wrapper<const function<vector<uint8_t>(const vector<uint8_t> &)>>
        service = null_ref;

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

  accept<http_handler>(service, ip::address_v4::from_string("0.0.0.0"), 9001);

  struct dummy_handler {};
  accept<dummy_handler>(service, ip::address_v4::from_string("0.0.0.0"), 9003);

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

  accept<ws_only_handler>(service, ip::address_v4::from_string("0.0.0.0"),
                          9002);

  vector<future<void>> threadpool;
  generate_n(back_inserter(threadpool), 10, [&service]() {
    return async(launch::async, [&service]() { service.run(); });
  });
  service.run();

  return 0;
}