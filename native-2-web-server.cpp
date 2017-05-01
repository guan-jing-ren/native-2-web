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

using namespace std;
using namespace std::experimental;
using namespace boost::asio;
using namespace beast;

template <typename Handler>
class n2w_connection : public enable_shared_from_this<n2w_connection<Handler>> {
  ip::tcp::socket socket;
  websocket::stream<ip::tcp::socket &> ws;
  io_service::strand strand;
  queue<shared_future<function<void()>>> write_queue;

  boost::asio::streambuf buf;
  http::request<http::string_body> request;
  http::response<http::string_body> response;
  websocket::opcode op;
  istream stream;
  string text_response;
  vector<uint8_t> binary_response;

  static auto websocket_support(...) -> false_type;
  template <typename T = Handler>
  static auto websocket_support(T *) -> typename T::websocket_handler_type;
  Handler handler;
  using websocket_handler_type =
      decltype(websocket_support(static_cast<Handler *>(nullptr)));
  static constexpr bool supports_websocket =
      !is_same<websocket_handler_type, false_type>::value;
  websocket_handler_type websocket_handler;

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

  void push_next_reply() {
    strand.dispatch([self = this->shared_from_this()]() {
      if (self->write_queue.empty())
        return; // No need to keep watching. A new async_write submitted to an
                // empty queue will kick this off.
      if (!self->write_queue.front().valid()) {
        self->push_next_reply(); // Keep watching periodically to check up on
                                 // current response.
        return;
      }
      auto reply = move(self->write_queue.front());
      self->write_queue.pop();
      reply.get()();
    });
  }

  void defer_response(shared_future<void> &&reply_future) {
    socket.get_io_service().post([reply_future]() { reply_future.get(); });
  }

  template <typename R> auto create_writer(const R &response) {
    static constexpr const auto response_type =
        is_same<string, R>::value ? "text" : "binary";
    static const auto message_type = websocket::message_type{
        is_same<string, R>::value ? websocket::opcode::text
                                  : websocket::opcode::binary};

    return [ self = this->shared_from_this(), &response ]() {
      self->ws.set_option(message_type);
      self->ws.async_write(buffer(response), [self = move(self)](auto ec) {
        clog << "Thread: " << this_thread::get_id() << "; Written "
             << response_type << "websocket response:" << ec << ".\n";
        if (ec) {
          self->websocket_handler = websocket_handler_type{};
          return;
        }
        self->push_next_reply();
      });
    };
  }

  void defer_response(shared_future<string> &&reply_future) {
    defer_response(async(launch::deferred, [
      self = this->shared_from_this(), reply_future = move(reply_future)
    ]()->function<void()> {
      return self->create_writer(self->text_response =
                                     move(reply_future.get()));
    }));
  }

  void defer_response(shared_future<vector<uint8_t>> &&reply_future) {
    defer_response(async(launch::deferred, [
      self = this->shared_from_this(), reply_future = move(reply_future)
    ]()->function<void()> {
      return self->create_writer(self->binary_response =
                                     move(reply_future.get()));
    }));
  }

  void defer_response(shared_future<function<void()>> &&reply_future) {
    strand.dispatch([ self = this->shared_from_this(), reply_future ]() {
      self->write_queue.push(move(reply_future));
      if (self->write_queue.size() == 1)
        self->push_next_reply(); // This starts the ball rolling. async_write
                                 // completion handlers runs the next write job,
                                 // but the first one submitted to an empty
                                 // queue does not have an async_write
                                 // previously to kick it off.
    });

    socket.get_io_service().post([reply_future]() { reply_future.get(); });
  }

  void respond() {
    auto reply_future =
        async(launch::deferred,
              [self = this->shared_from_this()]()->function<void()> {
                if (http::is_upgrade(self->request)) {
                  if (supports_websocket) {
                    self->ws.async_accept(self->request, [self = move(self)](
                                                             auto ec) mutable {
                      clog << "Accepted websocket connection: " << ec << '\n';
                      if (ec)
                        return;
                      self->stream >> noskipws;
                      self->register_websocket_pusher();
                      self->ws_serve();
                    });
                    return [self = move(self)]() { self->push_next_reply(); };
                  } else {
                    self->response = http::response<http::string_body>{};
                    self->response.version = self->request.version;
                    self->response.status = 501;
                    self->response.reason = "Not Implemented";
                    self->response.body =
                        "n2w server does implement websocket handling";
                  }
                } else
                  self->response = move(self->handler(self->request));

                prepare(self->response);
                clog << self->response;

                return [self = move(self)]() {
                  http::async_write(self->socket, self->response,
                                    [self = move(self)](auto ec) mutable {
                                      clog
                                          << "Thread: " << this_thread::get_id()
                                          << "; Written response:" << ec
                                          << ".\n";
                                      if (ec)
                                        return;
                                      self->push_next_reply();
                                    });
                };
              })
            .share();

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
      clog << self->request;
      self->respond();

      if (!supports_websocket || !http::is_upgrade(self->request))
        self->serve();
    });
  }

  template <typename T> void handle_websocket_request(false_type, T &&) {}
  template <typename H, typename T> auto handle_websocket_request(H &&, T &&t) {
    return async(launch::deferred,
                 [self = this->shared_from_this()](T && t) {
                   return self->websocket_handler(t);
                 },
                 move(t))
        .share();
  }

  template <typename H, bool B = websocket_handles_text>
  auto do_if_websocket_handles_text(H &&, string &&message) -> enable_if_t<!B> {
  }
  template <typename H, bool B = websocket_handles_text>
  auto do_if_websocket_handles_text(H &&, string &&message) -> enable_if_t<B> {
    defer_response(handle_websocket_request(websocket_handler, move(message)));
  }
  template <typename H, bool B = websocket_handles_binary>
  auto do_if_websocket_handles_binary(H &&, vector<uint8_t> &&message)
      -> enable_if_t<!B> {}
  template <typename H, bool B = websocket_handles_binary>
  auto do_if_websocket_handles_binary(H &&, vector<uint8_t> &&message)
      -> enable_if_t<B> {
    defer_response(handle_websocket_request(websocket_handler, move(message)));
  }

  void ws_serve() {
    ws.async_read(op, buf, [self = this->shared_from_this()](auto ec) {
      clog << "Read something from websocket: " << ec << '\n';
      if (ec)
        return;
      switch (self->op) {
      case websocket::opcode::close:
        return;
      default:
        clog << "Unsupported WebSocket opcode: "
             << static_cast<underlying_type_t<decltype(self->op)>>(self->op)
             << '\n';
        break;
      case websocket::opcode::text: {
        string message;
        copy(istream_iterator<char>(self->stream), {}, back_inserter(message));
        clog << "Text message received: " << message << '\n';
        self->do_if_websocket_handles_text(self->websocket_handler,
                                           move(message));
      } break;
      case websocket::opcode::binary: {
        vector<uint8_t> message;
        copy(istream_iterator<char>(self->stream), {}, back_inserter(message));
        clog << "Binary message received\n";
        self->do_if_websocket_handles_binary(self->websocket_handler,
                                             move(message));
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
    websocket_handler([self = this->shared_from_this()](string message) {
      promise<string> p;
      p.set_value(move(message));
      self->defer_response(p.get_future());
    });
  }

  template <bool B = websocket_pushes_binary>
  auto register_websocket_pusher_binary() -> enable_if_t<!B> {}
  template <bool B = websocket_pushes_binary>
  auto register_websocket_pusher_binary() -> enable_if_t<B> {
    websocket_handler([self =
                           this->shared_from_this()](vector<uint8_t> message) {
      promise<vector<uint8_t>> p;
      p.set_value(move(message));
      self->defer_response(p.get_future());
    });
  }

public:
  n2w_connection(io_service &service)
      : socket{service}, ws{socket}, strand{service}, stream{&buf} {}
};

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
    auto segments =
        accumulate(find_if_not(begin(path), end(path),
                               [](const auto &node) { return node == ".."; }),
                   end(path), vector<filesystem::path>{},
                   [](auto &v, const auto &p) {
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

int main() {
  io_service service;
  ip::tcp::endpoint endpoint{ip::address_v4::from_string("0.0.0.0"), 9001};
  ip::tcp::acceptor acceptor{service, endpoint, true};
  acceptor.listen();

  struct websocket_handler {
    vector<uint8_t> operator()(string message) { return {3, 1, 4, 1, 5}; }
    string operator()(vector<uint8_t> message) {
      return "Don't understand message\n";
    }
  };

  struct http_handler {
    using websocket_handler_type = websocket_handler;

    const filesystem::path web_root = filesystem::current_path();

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

      if (!filesystem::exists(path, ec)) {
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
          while (string_pusher) {
            string_pusher(
                to_string(i++) + ' ' +
                to_string(
                    chrono::system_clock::now().time_since_epoch().count()));
            this_thread::sleep_for(chrono::milliseconds{500});
          }
        });
        t.detach();
      }
    };
    http::response<http::string_body>
    operator()(const http::request<http::string_body> &) {
      return {};
    }
  };

  ip::tcp::endpoint wsendpoint{ip::address_v4::from_string("0.0.0.0"), 9002};
  ip::tcp::acceptor wsacceptor{service, wsendpoint, true};
  wsacceptor.listen();
  accept<ws_only_handler>(service, wsacceptor);

  vector<future<void>> threadpool;
  for (auto i = 0; i < 10; ++i)
    threadpool.emplace_back(
        async(launch::async, [&service]() { service.run(); }));

  return 0;
}