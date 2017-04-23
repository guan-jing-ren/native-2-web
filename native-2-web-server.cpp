#define BOOST_ERROR_CODE_HEADER_ONLY
#include <boost/system/error_code.hpp>
inline bool operator==(boost::system::error_code ec, int i) { return ec == i; }
inline bool operator!=(boost::system::error_code ec, int i) { return ec != i; }

#include <beast/http.hpp>
#include <beast/websocket.hpp>
#include <boost/asio.hpp>

#include <future>
#include <iostream>
#include <memory>

using namespace std;
using namespace boost::asio;
using namespace beast;

struct n2w_connection {
  ip::tcp::socket socket;
  io_service::strand strand;
  boost::asio::streambuf buf;
  http::request<http::string_body> request;
  http::response<http::string_body> response;

  n2w_connection(io_service &service) : socket{service}, strand{service} {}
};

void serve(shared_ptr<n2w_connection> connection);

void respond(shared_ptr<n2w_connection> connection) {
  auto &strand_ref = connection->strand;
  auto &socket_ref = connection->socket;
  auto &response_ref = connection->response;
  response_ref = decltype(connection->response)();
  response_ref.version = connection->request.version;
  response_ref.status = 200;
  response_ref.reason = "OK";
  prepare(response_ref);
  beast::http::async_write(
      socket_ref, response_ref, [connection = move(connection)](auto ec) {
        clog << "Thread: " << this_thread::get_id()
             << "; Written response:" << ec << ".\n";
        if (ec)
          return;
        clog << connection->response;
        auto &strand_ref = connection->strand;
        strand_ref.dispatch([connection = move(connection)]() {
          serve(move(connection));
        });
      });
}

void serve(shared_ptr<n2w_connection> connection) {
  auto &strand_ref = connection->strand;
  auto &socket_ref = connection->socket;
  auto &buf_ref = connection->buf;
  auto &request_ref = connection->request;
  request_ref = decltype(connection->request)();
  beast::http::async_read(
      socket_ref, buf_ref, request_ref,
      [connection = move(connection)](auto ec) {
        clog << "Thread: " << this_thread::get_id()
             << "; Received request: " << ec << ".\n";
        if (ec)
          return;
        clog << connection->request;
        auto &strand_ref = connection->strand;
        strand_ref.dispatch([connection = move(connection)]() {
          respond(move(connection));
        });
      });
}

void accept(io_service &service, ip::tcp::acceptor &acceptor) {
  auto connection = make_shared<n2w_connection>(service);
  auto &socket_ref = connection->socket;
  acceptor.async_accept(
      socket_ref,
      [&service, &acceptor, connection = move(connection) ](auto ec) mutable {
        clog << "Thread: " << this_thread::get_id()
             << "; Accepted connection: " << ec << '\n';
        if (ec)
          return;
        accept(service, acceptor);
        serve(move(connection));
      });
}

int main() {
  io_service service;
  ip::tcp::endpoint endpoint{ip::address_v4::from_string("0.0.0.0"), 9001};
  ip::tcp::acceptor acceptor{service, endpoint, true};
  acceptor.listen();

  accept(service, acceptor);

  vector<future<void>> threadpool;
  for (auto i = 0; i < 10; ++i)
    threadpool.emplace_back(
        async(launch::async, [&service]() { service.run(); }));

  return 0;
}