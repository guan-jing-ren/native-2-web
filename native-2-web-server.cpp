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
  boost::asio::streambuf buf;
  http::request<http::string_body> request;
  websocket::opcode op;
  istream stream;

  Handler handler;

  template <typename> friend void accept(io_service &, ip::tcp::acceptor &);

  void respond() {
    auto response = handler(request);
    prepare(response);
    clog << response;
    http::async_write(
        socket, response, [self = this->shared_from_this()](auto ec) mutable {
          clog << "Thread: " << this_thread::get_id()
               << "; Written response:" << ec << ".\n";
          if (ec)
            return;
          auto &strand_ref = self->strand;
          strand_ref.dispatch([self = move(self)]() mutable { self->serve(); });
        });
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
      auto &strand_ref = self->strand;

      auto &self_ref = *self;
      if (!http::is_upgrade(self_ref.request))
        strand_ref.dispatch([self = move(self)]() mutable { self->respond(); });
      else {
        self_ref.ws.set_option(
            websocket::message_type{websocket::opcode::binary});
        self_ref.ws.async_accept(
            self_ref.request, [self = move(self)](auto ec) mutable {
              clog << "Accepted websocket connection: " << ec << '\n';
              if (ec)
                return;
              self->stream >> noskipws;
              self->ws_serve();
            });
      }
    });
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

  struct http_handler {
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