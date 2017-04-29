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
  websocket::opcode op;
  istream stream;

  static auto websocket_support(...) -> false_type;
  template <typename T = Handler>
  static auto websocket_support(T *) -> typename T::websocket_handler_type;
  Handler handler;
  using websocket_handler_type =
      decltype(websocket_support(static_cast<Handler *>(nullptr)));
  static constexpr bool supports_websocket =
      !is_same<websocket_handler_type, false_type>::value;
  websocket_handler_type websocket_handler;

  template <typename> friend void accept(io_service &, ip::tcp::acceptor &);

  void push_next_reply() {
    strand.dispatch([self = this->shared_from_this()]() {
      if (self->write_queue.empty())
        return;
      if (!self->write_queue.front().valid()) {
        self->push_next_reply();
        return;
      }
      auto reply = move(self->write_queue.front());
      self->write_queue.pop();
      reply.get()();
    });
  }

  void respond() {
    auto reply_future =
        async(launch::deferred,
              [self = this->shared_from_this()]()->function<void()> {
                http::response<http::string_body> response;
                if (http::is_upgrade(self->request)) {
                  if (supports_websocket) {
                    self->ws.set_option(
                        websocket::message_type{websocket::opcode::binary});
                    self->ws.async_accept(self->request, [self = move(self)](
                                                             auto ec) mutable {
                      clog << "Accepted websocket connection: " << ec << '\n';
                      if (ec)
                        return;
                      self->stream >> noskipws;
                      self->ws_serve();
                    });
                    return []() {};
                  } else {
                    response.version = self->request.version;
                    response.status = 501;
                    response.reason = "Not Implemented";
                    response.body =
                        "n2w server does implement websocket handling";
                  }
                } else
                  response = self->handler(self->request);

                prepare(response);
                clog << response;

                return [ self = move(self), response = move(response) ]() {
                  http::async_write(self->socket, response,
                                    [self = move(self)](auto ec) mutable {
                                      clog
                                          << "Thread: " << this_thread::get_id()
                                          << "; Written response:" << ec
                                          << ".\n";
                                      self->push_next_reply();
                                      if (ec)
                                        return;
                                    });
                };
              })
            .share();

    strand.dispatch([ self = this->shared_from_this(), reply_future ]() {
      self->write_queue.push(move(reply_future));
      if (self->write_queue.size() == 1)
        self->push_next_reply();
    });

    socket.get_io_service().post([reply_future]() { reply_future.get(); });

    if (!supports_websocket || !http::is_upgrade(request))
      serve();
  }

  void serve() {
    request = decltype(request)();
    http::async_read(socket, buf, request,
                     [self = this->shared_from_this()](auto ec) mutable {
                       clog << "Thread: " << this_thread::get_id()
                            << "; Received request: " << ec << ".\n";
                       if (ec)
                         return;
                       clog << self->request;
                       self->respond();
                     });
  }

  template <typename T> void handle_websocket(false_type, T &&) {}
  template <typename H, typename T> void handle_websocket(H &&, T &&t) {
    websocket_handler(move(t));
  }

  void ws_serve() {
    ws.async_read(op, buf, [self = this->shared_from_this()](auto ec) {
      clog << "Read something from websocket: " << ec << '\n';
      if (ec)
        return;
      switch (self->op) {
      default:
        clog << "Unsupported WebSocket opcode: "
             << static_cast<underlying_type_t<decltype(self->op)>>(self->op)
             << '\n';
        break;
      case websocket::opcode::text: {
        string message;
        copy(istream_iterator<char>(self->stream), {}, back_inserter(message));
        clog << "Text message received: " << message << '\n';
        self->handle_websocket(self->websocket_handler, move(message));
      } break;
      case websocket::opcode::binary: {
        vector<uint8_t> message;
        copy(istream_iterator<char>(self->stream), {}, back_inserter(message));
        clog << "Binary message received\n";
        self->handle_websocket(self->websocket_handler, move(message));
      } break;
      }
      self->ws_serve();
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
    void operator()(string message) {}
    void operator()(vector<uint8_t> message) {}
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

  vector<future<void>> threadpool;
  for (auto i = 0; i < 10; ++i)
    threadpool.emplace_back(
        async(launch::async, [&service]() { service.run(); }));

  return 0;
}