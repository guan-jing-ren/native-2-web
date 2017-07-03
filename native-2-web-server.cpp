#define BOOST_ERROR_CODE_HEADER_ONLY
#include <boost/system/error_code.hpp>
inline bool operator==(boost::system::error_code ec, int i) { return ec == i; }
inline bool operator!=(boost::system::error_code ec, int i) { return ec != i; }

#include <beast/http.hpp>
#include <beast/websocket.hpp>
#include <boost/asio.hpp>

#include <algorithm>
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
  ip::tcp::socket socket;
  websocket::stream<ip::tcp::socket &> ws;
  atomic_uintmax_t ticket{0};
  atomic_uintmax_t serving{0};

  boost::asio::streambuf buf;
  http::request<http::string_body> request;
  http::response<http::string_body> response;

  struct NullHandler {
    http::response<http::string_body>
    operator()(const http::request<http::string_body> &request) {
      auto response = http::response<http::string_body>{};
      response.version = request.version;
      response.status = 501;
      response.reason = "Not Implemented";
      response.body = "n2w server does implement websocket handling";
      return response;
    }
  };

  static auto http_support(...) -> false_type;
  template <typename T = Handler>
  static auto http_support(T *)
      -> decltype(T{}(http::request<http::string_body>{}));
  static constexpr bool supports_http =
      !is_same<decltype(http_support(static_cast<Handler *>(nullptr))),
               false_type>::value;
  conditional_t<supports_http, Handler, NullHandler> handler;

  static auto websocket_support(...) -> false_type;
  template <typename T = Handler>
  static auto websocket_support(T *) -> typename T::websocket_handler_type;
  using websocket_handler_type =
      decltype(websocket_support(static_cast<Handler *>(nullptr)));
  static constexpr bool supports_websocket =
      !is_same<websocket_handler_type, false_type>::value;
  struct websocket_stuff {
    websocket_handler_type websocket_handler;
    websocket::opcode op;
    istream stream;
    string text_response;
    vector<uint8_t> binary_response;
    websocket_stuff(boost::asio::streambuf *buf) : stream(buf) {}
  };
  conditional_t<supports_websocket, websocket_stuff, void *> ws_stuff{&buf};

  template <typename> static auto websocket_handles(...) -> false_type;
  template <typename V, typename T = Handler>
  static auto websocket_handles(T *)
      -> decltype(async(launch::deferred, typename T::websocket_handler_type{},
                        V{}));
  static constexpr bool websocket_handles_text =
      supports_websocket &&
      !is_same<decltype(
                   websocket_handles<string>(static_cast<Handler *>(nullptr))),
               false_type>::value;
  static constexpr bool websocket_handles_binary =
      supports_websocket &&
      !is_same<decltype(websocket_handles<vector<uint8_t>>(
                   static_cast<Handler *>(nullptr))),
               false_type>::value;

  template <typename> static auto websocket_pushes(...) -> false_type;
  template <typename V, typename T = Handler>
  static auto websocket_pushes(T *)
      -> decltype(async(launch::deferred, typename T::websocket_handler_type{},
                        function<void(V)>{}));
  static constexpr bool websocket_pushes_text =
      supports_websocket &&
      !is_same<decltype(
                   websocket_pushes<string>(static_cast<Handler *>(nullptr))),
               false_type>::value;
  static constexpr bool websocket_pushes_binary =
      supports_websocket &&
      !is_same<decltype(websocket_pushes<vector<uint8_t>>(
                   static_cast<Handler *>(nullptr))),
               false_type>::value;

  template <typename> friend void accept(io_service &, ip::tcp::acceptor &);

  void defer_response(shared_future<void> &&reply_future) {
    socket.get_io_service().post(
        [ reply_future, self = this->shared_from_this() ]() mutable {
          try {
            reply_future.get();
          } catch (...) {
            clog << "Exception thrown from void future\n";
            self->ws.close({4000, "Exception thrown from void future"});
          }
        });
  }

  template <typename R> auto create_writer(const R &response) {
    static constexpr const auto response_type =
        is_same<string, R>::value ? "text" : "binary";
    static const auto message_type = websocket::message_type{
        is_same<string, R>::value ? websocket::opcode::text
                                  : websocket::opcode::binary};

    return [ self = this->shared_from_this(), &response ]() mutable {
      self->ws.set_option(message_type);
      self->ws.async_write(buffer(response), [self](auto ec) mutable {
        clog << "Thread: " << this_thread::get_id() << "; Written "
             << response_type << " websocket response:" << ec << ".\n";
        ++self->serving;
        if (ec)
          self->ws_stuff.websocket_handler = websocket_handler_type{};
      });
    };
  }

  void defer_response(shared_future<string> &&reply_future) {
    defer_response(async(launch::deferred, [
      self = this->shared_from_this(), reply_future = move(reply_future)
    ]() mutable->function<void()> {
      try {
        return self->create_writer(self->ws_stuff.text_response =
                                       move(reply_future.get()));
      } catch (...) {
        clog << "Exception thrown from string future\n";
        self->ws.close({4000, "Exception thrown from string future"});
        return []() {};
      }
    }));
  }

  void defer_response(shared_future<vector<uint8_t>> &&reply_future) {
    defer_response(async(launch::deferred, [
      self = this->shared_from_this(), reply_future = move(reply_future)
    ]() mutable->function<void()> {
      try {
        return self->create_writer(self->ws_stuff.binary_response =
                                       move(reply_future.get()));
      } catch (...) {
        clog << "Exception thrown from binary future\n";
        self->ws.close({4000, "Exception thrown from binary future"});
        return []() {};
      }
    }));
  }

  auto create_writer(const http::response<http::string_body> &response) {
    return [self = this->shared_from_this()]() mutable {
      http::async_write(self->socket, self->response, [self](auto ec) mutable {
        clog << "Thread: " << this_thread::get_id()
             << "; Written HTTP response:" << ec << ".\n";
        ++self->serving;
      });
    };
  }

  void push_next_reply(shared_future<function<void()>> reply_future,
                       uintmax_t tkt) {
    socket.get_io_service().post([
      self = this->shared_from_this(), reply_future, ticket = tkt
    ]() mutable {
      clog << "Waiting for ticket: " << ticket
           << ", currently serving: " << self->serving
           << ", has future: " << boolalpha << reply_future.valid()
           << noboolalpha << '\n';
      if (!reply_future.valid() || self->serving != ticket) {
        self->push_next_reply(move(reply_future), ticket);
        return;
      }
      reply_future.get()();
    });
  }

  void defer_response(shared_future<function<void()>> &&reply_future) {
    push_next_reply(reply_future, ticket++);
    socket.get_io_service().post(
        [reply_future]() mutable { reply_future.get(); });
  }

  template <bool B = supports_websocket> auto accept_ws() -> enable_if_t<!B> {}
  template <bool B = supports_websocket> auto accept_ws() -> enable_if_t<B> {
    ws.async_accept(
        request, [self = this->shared_from_this()](auto ec) mutable {
          clog << "Accepted websocket connection: " << ec << '\n';
          if (ec)
            return;
          self->ws_stuff.stream >> noskipws;
          self->register_websocket_pusher();
          self->ws_serve();
        });
  }

  void respond() {
    auto reply_future = async(launch::deferred, [
                          self = this->shared_from_this(), request = request
                        ]() mutable->function<void()> {
                          if (http::is_upgrade(request)) {
                            if (supports_websocket) {
                              self->accept_ws();
                              return [self = move(self)]() mutable {
                                ++self->serving;
                              };
                            } else
                              self->response = move(NullHandler{}(request));
                          } else
                            self->response = move(self->handler(request));

                          prepare(self->response);
                          clog << self->response;

                          return self->create_writer(self->response);
                        }).share();
    defer_response(move(reply_future));
  }

  void serve() {
    request = decltype(request)();
    http::async_read(socket, buf, request, [self = this->shared_from_this()](
                                               auto ec) mutable {
      clog << "Thread: " << this_thread::get_id()
           << "; Received request: " << ec << ".\n";
      if (ec)
        return;
      self->respond();

      if (!supports_websocket || !http::is_upgrade(self->request))
        self->serve();
    });
  }

  template <typename T> void handle_websocket_request(false_type, T &&) {}
  template <typename H, typename T> auto handle_websocket_request(H &&, T &&t) {
    return async(launch::deferred,
                 [self = this->shared_from_this()](T && t) mutable {
                   return self->ws_stuff.websocket_handler(t);
                 },
                 move(t))
        .share();
  }

  template <typename H, bool B = websocket_handles_text>
  auto do_if_websocket_handles_text(H &&, string &&message) -> enable_if_t<!B> {
  }
  template <typename H, bool B = websocket_handles_text>
  auto do_if_websocket_handles_text(H &&, string &&message) -> enable_if_t<B> {
    defer_response(
        handle_websocket_request(ws_stuff.websocket_handler, move(message)));
  }
  template <typename H, bool B = websocket_handles_binary>
  auto do_if_websocket_handles_binary(H &&, vector<uint8_t> &&message)
      -> enable_if_t<!B> {}
  template <typename H, bool B = websocket_handles_binary>
  auto do_if_websocket_handles_binary(H &&, vector<uint8_t> &&message)
      -> enable_if_t<B> {
    defer_response(
        handle_websocket_request(ws_stuff.websocket_handler, move(message)));
  }

  template <bool B = supports_websocket> auto ws_serve() -> enable_if_t<!B> {}
  template <bool B = supports_websocket> auto ws_serve() -> enable_if_t<B> {
    ws.async_read(
        ws_stuff.op, buf, [self = this->shared_from_this()](auto ec) mutable {
          clog << "Read something from websocket: " << ec << '\n';
          if (ec)
            return;
          switch (self->ws_stuff.op) {
          case websocket::opcode::close:
            return;
          default:
            clog << "Unsupported WebSocket opcode: "
                 << static_cast<underlying_type_t<decltype(self->ws_stuff.op)>>(
                        self->ws_stuff.op)
                 << '\n';
            break;
          case websocket::opcode::text: {
            string message;
            copy(istream_iterator<char>(self->ws_stuff.stream), {},
                 back_inserter(message));
            self->ws_stuff.stream.clear();
            clog << "Text message received: " << message << '\n';
            self->do_if_websocket_handles_text(self->ws_stuff.websocket_handler,
                                               move(message));
          } break;
          case websocket::opcode::binary: {
            vector<uint8_t> message;
            copy(istream_iterator<char>(self->ws_stuff.stream), {},
                 back_inserter(message));
            self->ws_stuff.stream.clear();
            clog << "Binary message received, size: " << message.size() << '\n';
            self->do_if_websocket_handles_binary(
                self->ws_stuff.websocket_handler, move(message));
          } break;
          }

          self->ws_serve();
        });
  }

  template <bool B = websocket_pushes_text || websocket_pushes_binary>
  auto register_websocket_pusher() -> enable_if_t<!B> {}
  template <bool B = websocket_pushes_text || websocket_pushes_binary>
  auto register_websocket_pusher() -> enable_if_t<B> {
    register_websocket_pusher_text();
    register_websocket_pusher_binary();
  }

  template <bool B = websocket_pushes_text>
  auto register_websocket_pusher_text() -> enable_if_t<!B> {}
  template <bool B = websocket_pushes_text>
  auto register_websocket_pusher_text() -> enable_if_t<B> {
    ws_stuff.websocket_handler([self = this->shared_from_this()](
        string message) mutable {
      promise<string> p;
      p.set_value(move(message));
      self->defer_response(p.get_future());
    });
  }

  template <bool B = websocket_pushes_binary>
  auto register_websocket_pusher_binary() -> enable_if_t<!B> {}
  template <bool B = websocket_pushes_binary>
  auto register_websocket_pusher_binary() -> enable_if_t<B> {
    websocket_handler([self = this->shared_from_this()](
        vector<uint8_t> message) mutable {
      promise<vector<uint8_t>> p;
      p.set_value(move(message));
      self->defer_response(p.get_future());
    });
  }

public:
  n2w_connection(io_service &service)
      : socket{service}, ws{socket}, ws_stuff(&buf) {}
};

template <typename Handler>
void check_use_count(io_service &service,
                     weak_ptr<n2w_connection<Handler>> conn,
                     uintptr_t conn_id) {
  static basic_waitable_timer<chrono::system_clock> timer{service};
  timer.expires_from_now(chrono::seconds{1});

  service.post([
    &service, conn = weak_ptr<n2w_connection<Handler>>(conn), conn_id
  ]() mutable {
    clog << "Connection " << conn_id << " use count: " << conn.use_count()
         << '\n';
    if (conn.expired() || conn.use_count() < 2)
      return;

    timer.async_wait([&service, conn = move(conn), conn_id ](auto ec) mutable {
      if (ec == error::operation_aborted)
        return;
      check_use_count(service, move(conn), conn_id);
    });
  });
}

template <typename Handler>
void accept(io_service &service, ip::tcp::acceptor &acceptor) {
  auto connection = make_shared<n2w_connection<Handler>>(service);
  auto &socket_ref = connection->socket;
  acceptor.async_accept(
      socket_ref,
      [&service, &acceptor, connection = move(connection) ](auto ec) mutable {
        clog << "Thread: " << this_thread::get_id()
             << "; Accepted connection: " << ec << '\n';
        if (ec)
          return;
        accept<Handler>(service, acceptor);
        connection->serve();
        check_use_count<Handler>(service, connection,
                                 reinterpret_cast<uintptr_t>(connection.get()));
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
    vector<string> hierarchy;
    auto first = begin(path);
    advance(first, distance(cbegin(web_root), cend(web_root)));
    path.replace_extension("");
    transform(first, end(path), back_inserter(hierarchy),
              mem_fn(&filesystem::path::generic_u8string));
    transform(
        cbegin(hierarchy), cend(hierarchy),
        begin(hierarchy), [offset = strlen("libn2w-")](const auto &module) {
          return module.substr(offset);
        });
    plugins.emplace(hierarchy, modfile.c_str());
  }
}

n2w::plugin server = []() {
  n2w::plugin server;
  server.register_service(DECLARE_API(reload_plugins), "");
  return server;
}();

string create_modules() {
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
  ip::tcp::endpoint endpoint{ip::address_v4::from_string("0.0.0.0"), 9001};
  ip::tcp::acceptor acceptor{service, endpoint, true};
  acceptor.listen();

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
      normalized_uri uri{request.url};
      auto path = web_root / uri.path;
      clog << "Thread: " << this_thread::get_id()
           << "; Root directory: " << web_root << ", URI: " << request.url
           << ", Path: " << path << '\n';
      std::error_code ec;
      if (equivalent(path, web_root, ec))
        path /= "test.html";

      http::response<http::string_body> response;
      response.version = request.version;
      response.status = 200;
      response.reason = "OK";

      if (path == web_root / "modules.js") {
        response.fields.insert("Content-Type", "javascript");
        response.body = create_modules();
        cerr << response.body;
      } else if (!filesystem::exists(path, ec)) {
        response.status = 404;
        response.reason = "Not Found";
        response.body = ec.message();
      } else if (!filesystem::is_regular_file(path, ec)) {
        response.status = 403;
        response.reason = "Forbidden";
        response.body = ec ? ec.message() : "Path is not a regular file";
      } else {
        auto extension = path.extension();
        if (extension != ".html" && extension != ".htm" && extension != ".js" &&
            extension != ".css") {
          response.status = 406;
          response.reason = "Not Acceptable";
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
          response.fields.insert("Content-Type", type);
        }
      }

      return response;
    }
  };

  accept<http_handler>(service, acceptor);

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

  ip::tcp::endpoint wsendpoint{ip::address_v4::from_string("0.0.0.0"), 9002};
  ip::tcp::acceptor wsacceptor{service, wsendpoint, true};
  wsacceptor.listen();
  accept<ws_only_handler>(service, wsacceptor);

  vector<future<void>> threadpool;
  for (auto i = 0; i < 10; ++i)
    threadpool.emplace_back(
        async(launch::async, [&service]() mutable { service.run(); }));

  return 0;
}