#ifndef _NATIVE_2_WEB_CONNECTION_HPP_
#define _NATIVE_2_WEB_CONNECTION_HPP_

#define BOOST_ERROR_CODE_HEADER_ONLY
#include <boost/system/error_code.hpp>
inline bool operator==(boost::system::error_code ec, int i) { return ec == i; }
inline bool operator!=(boost::system::error_code ec, int i) { return ec != i; }

#include <beast/http.hpp>
#include <beast/websocket.hpp>
#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/preprocessor.hpp>
#define BOOST_COROUTINES_NO_DEPRECATION_WARNING 1
#define BOOST_COROUTINES_V2 1
#include <boost/asio/spawn.hpp>

#include <experimental/filesystem>
#include <thread>

namespace n2w {

namespace connection_detail {
using namespace std;
using namespace experimental;
using namespace boost::asio;
using namespace beast;

template <typename> class client_connection;

template <typename Handler>
class connection final : public enable_shared_from_this<connection<Handler>> {

  struct unsupported;

#define N2W__SUPPORT_DECLVAL(r, data, i, elem)                                 \
  BOOST_PP_COMMA_IF(i) declval<elem>()

#define N2W__SUPPORT_ARGS(a) BOOST_PP_SEQ_FOR_EACH_I(N2W__SUPPORT_DECLVAL, _, a)

#define N2W__SUPPORT_IMPL(dependent, concept, handler, func, ...)              \
  static auto concept##_impl(...)->unsupported;                                \
  template <typename T = Handler>                                              \
  static auto concept##_impl(T *)->decltype(declval<handler>().func(           \
      N2W__SUPPORT_ARGS(BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))));              \
  static constexpr bool concept =                                              \
      dependent &&                                                             \
      !is_same_v<decltype(concept##_impl(declval<Handler *>())), unsupported>

#define N2W__SUPPORT(concept, handler, func, ...)                              \
  N2W__SUPPORT_IMPL(true, concept, handler, func, __VA_ARGS__)
#define N2W__SUPPORT_IF N2W__SUPPORT_IMPL

  /***************************/
  /* HTTP SFINAE DEFINITIONS */
  /***************************/

  N2W__SUPPORT(supports_http_receive, T, operator(),
               http::request<http::string_body>);

  struct adaptable;
  N2W__SUPPORT(supports_http_send_empty, T, operator(), filesystem::path);
  N2W__SUPPORT(supports_http_send_body, T, operator(), filesystem::path,
               adaptable);
  static constexpr bool supports_http_send =
      supports_http_send_empty || supports_http_send_body;

  static constexpr bool supports_http =
      supports_http_receive ^ supports_http_send;

  /**************************************/
  /* BASIC WEBSOCKET SFINAE DEFINITIONS */
  /**************************************/

  static auto websocket_support(...) -> unsupported;
  template <typename T = Handler>
  static auto websocket_support(T *) -> typename T::websocket_handler_type;
  using websocket_handler_type =
      decltype(websocket_support(declval<Handler *>()));
  static constexpr bool supports_websocket =
      !is_same_v<websocket_handler_type, unsupported>;

  N2W__SUPPORT(websocket_handles_text,
               typename T::websocket_handler_type, operator(), string);

  N2W__SUPPORT(websocket_handles_binary,
               typename T::websocket_handler_type, operator(), vector<uint8_t>);

  N2W__SUPPORT(supports_response_decoration, typename T::websocket_handler_type,
               decorate, http::request<http::string_body> &,
               http::response<http::string_body> &);

  N2W__SUPPORT(supports_request_decoration, typename T::websocket_handler_type,
               decorate, http::request<http::empty_body> &);

  N2W__SUPPORT(supports_upgrade_inspection, typename T::websocket_handler_type,
               inspect, http::response<http::string_body> &);

  /*****************************************/
  /* EXTENDED WEBSOCKET SFINAE DEFINITIONS */
  /*****************************************/

  N2W__SUPPORT(websocket_pushes_text,
               typename T::websocket_handler_type, operator(),
               function<void(string)>);

  N2W__SUPPORT(websocket_pushes_binary,
               typename T::websocket_handler_type, operator(),
               function<void(vector<uint8_t>)>);

  /*********************************/
  /* STATISTICS SFINAE DEFINITIONS */
  /*********************************/

  N2W__SUPPORT_IF(supports_http, reports_task_start, T, report_task_start,
                  chrono::system_clock::time_point);
  N2W__SUPPORT_IF(supports_http, reports_task_end, T, report_task_end,
                  chrono::system_clock::time_point);
  N2W__SUPPORT_IF(supports_http, reports_accept, T, report_accept,
                  chrono::system_clock::time_point);
  N2W__SUPPORT_IF(supports_http, reports_connect, T, report_connect,
                  chrono::system_clock::time_point);
  N2W__SUPPORT_IF(supports_http, reports_upgrade, T, report_upgrade,
                  chrono::system_clock::time_point);
  N2W__SUPPORT_IF(supports_http, reports_close, T, report_close,
                  chrono::system_clock::time_point, bool);
  N2W__SUPPORT_IF(supports_http, reports_error, T, report_error,
                  boost::system::error_code);

  /**********************************/
  /* INTERNAL STRUCTURE DEFINITIONS */
  /**********************************/

  struct adaptable {
    template <typename T> operator T();
  };

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
    bool open = false;
    websocket_handler_type websocket_handler;
    istream stream;
    websocket_stuff(boost::asio::streambuf *buf) : stream(buf) {}
  };

  struct private_construction_tag {};

  struct ticket_sentinel {
    reference_wrapper<connection> conn;
    const uintmax_t tkt;
    ticket_sentinel(connection &conn, uintmax_t tkt) : conn(conn), tkt(tkt) {}
    ~ticket_sentinel() { ++conn.get().serving; }

    bool operator!() const { return tkt != conn.get().serving; }
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
  void write_response(yield_context yield, R reply,
                      const ticket_sentinel &sentinel) {
    boost::system::error_code ec;
    while (!sentinel) {
      clog << "Waiting to serve: " << sentinel.tkt
           << ", currently serving: " << serving << ", is open: " << boolalpha
           << socket.is_open() << '\n';
      socket.get_io_service().post(yield[ec]);
    }

    string response_type;
    if constexpr (is_same_v<R, http::response<http::string_body>>) {
      reply.prepare_payload();
      http::async_write(socket, reply, yield[ec]);
      response_type = "HTTP";
    } else if constexpr (!is_same_v<R, nullptr_t> && supports_websocket) {
      if constexpr (is_same_v<R, string>) {
        ws.text(true);
        response_type = "text websocket";
      } else if constexpr (is_same_v<R, vector<uint8_t>>) {
        ws.binary(true);
        response_type = "binary websocket";
      }
      ws.async_write(buffer(reply), yield[ec]);
      if (ec)
        ws_stuff.websocket_handler = websocket_handler_type{};
    }

    clog << "Thread: " << this_thread::get_id() << "; Written " << response_type
         << " response: " << ec.message() << ".\n";
  }

  template <typename T> auto async(T t) {
    if constexpr (is_same_v<T, http::response<http::string_body>> ||
                  is_same_v<T, string> || is_same_v<T, vector<uint8_t>>) {
      spawn(socket.get_io_service(),
            [
              this, self = this->shared_from_this(), t = forward<T>(t),
              tkt = ticket++
            ](yield_context yield) {
              if constexpr (reports_task_start)
                handler.report_task_start(chrono::system_clock::now());
              ticket_sentinel sentinel{*this, tkt};
              write_response(yield, move(t), sentinel);
              if constexpr (reports_task_end)
                handler.report_task_end(chrono::system_clock::now());
            },
            boost::coroutines::attributes{12 << 10});
    } else {
      spawn(socket.get_io_service(),
            [
              this, self = this->shared_from_this(), t = forward<T>(t),
              tkt = ticket++
            ](yield_context yield) {
              if constexpr (reports_task_start)
                handler.report_task_start(chrono::system_clock::now());
              ticket_sentinel sentinel{*this, tkt};
              if constexpr (is_void_v<result_of_t<T()>>) {
                t();
                write_response(yield, nullptr, sentinel);
              } else
                write_response(yield, t(), sentinel);
              if constexpr (reports_task_end)
                handler.report_task_end(chrono::system_clock::now());
            },
            boost::coroutines::attributes{16 << 10});
    }
  }

  void serve(yield_context yield) {
    socket.set_option(ip::tcp::no_delay{true});

    boost::system::error_code ec;
    while (true) {
      http::request<http::string_body> request;
      http::async_read(socket, buf, request, yield[ec]);
      clog << "Thread: " << this_thread::get_id()
           << "; Received request: " << ec.message() << ".\n";
      clog << request;
      if (ec)
        break;

      if (websocket::is_upgrade(request)) {
        if constexpr (supports_websocket) {
          if constexpr (supports_response_decoration) {
            ws.async_accept_ex(request,
                               [this, request](auto &response) {
                                 ws_stuff.websocket_handler.decorate(request,
                                                                     response);
                               },
                               yield[ec]);
          } else {
            ws.async_accept(request, yield[ec]);
          }
          clog << "Accepted websocket connection: " << ec.message() << '\n';
          if (ec)
            break;
          ws_stuff.stream >> noskipws;
          register_websocket_pusher();
          goto do_upgrade;
        } else
          async(NullHandler{}(request));
        break;
      }

      if constexpr (supports_http_receive)
        async([ this, request = move(request) ]() { return handler(request); });
      else {
        async(NullHandler{}(request));
        break;
      }
    }
    clog << "Finished serving socket\n";

    return;
  do_upgrade:
    ws_serve(ec, yield);
    clog << "Finished serving websocket\n";
  }

  void ws_serve(boost::system::error_code ec, yield_context yield) {
    if constexpr (supports_websocket) {
      if constexpr (reports_task_start)
        handler.report_task_start(chrono::system_clock::now());
      if constexpr (reports_upgrade) {
        ws_stuff.open = true;
        handler.report_upgrade(chrono::system_clock::now());
      }

      auto save_stream = [this](auto &message) {
        copy(istream_iterator<char>(ws_stuff.stream), {},
             back_inserter(message));
        ws_stuff.stream.clear();
      };

      while (true) {
        ws.async_read(buf, yield[ec]);
        clog << "Read something from websocket: " << ec.message() << '\n';
        if (ec)
          break;
        if (ws.got_text()) {
          string message;
          save_stream(message);
          clog << "Text message received: " << message << '\n';
          if constexpr (websocket_handles_text)
            async([ this, message = move(message) ] {
              return ws_stuff.websocket_handler(message);
            });
        } else if (ws.got_binary()) {
          vector<uint8_t> message;
          save_stream(message);
          clog << "Binary message received, size: " << message.size() << '\n';
          if constexpr (websocket_handles_binary)
            async([ this, message = move(message) ] {
              return ws_stuff.websocket_handler(message);
            });
        } else {
          clog << "Unsupported WebSocket opcode: " << '\n';
          break;
        }
      }
      if constexpr (reports_task_end)
        handler.report_task_end(chrono::system_clock::now());
    }
  }

  auto register_websocket_pusher() {
    auto pusher =
        [ this, self = this->shared_from_this() ](auto message) mutable {
      if (!self)
        return;
      if (self.use_count() == 1)
        self.reset();
      async(message);
    };

    if constexpr (websocket_pushes_text)
      ws_stuff.websocket_handler(pusher);
    if constexpr (websocket_pushes_binary)
      ws_stuff.websocket_handler(pusher);
  }

  /***********************/
  /* EXTERNAL INTERFACES */
  /***********************/

  template <typename, typename... Args>
  friend void accept(reference_wrapper<io_service> service, Args &&... args);

  template <typename> friend class client_connection;
  template <typename> friend class wsclient_connection;

  template <typename H, typename... Args>
  friend client_connection<H> connect(reference_wrapper<io_service> service,
                                      Args &&... args);

  /********************************/
  /* CLIENT CONNECTION INTERFACES */
  /********************************/

  template <typename CompletionHandler>
  auto request(const filesystem::path p, CompletionHandler &&ch) {
    if constexpr (supports_http_send) {
      async_completion<CompletionHandler,
                       void(boost::system::error_code,
                            http::response<http::string_body>)>
          completion{ch};
      spawn(socket.get_io_service(),
            [
              this, p, handler = move(completion.completion_handler),
              tkt = ticket++
            ](yield_context yield) mutable {
              if constexpr (reports_task_start)
                this->handler.report_task_start(chrono::system_clock::now());
              ticket_sentinel sentinel{*this, tkt};
              while (!sentinel)
                socket.get_io_service().post(yield);

              boost::system::error_code ec;
              auto req = this->handler(p);
              req.method(http::verb::get);
              req.prepare_payload();
              http::async_write(socket, req, yield[ec]);
              clog << "Thread: " << this_thread::get_id()
                   << "; Sent request: " << ec.message() << ".\n";
              http::response<http::string_body> res;
              http::async_read(socket, buf, res, yield[ec]);
              clog << "Thread: " << this_thread::get_id()
                   << "; Received response: " << ec.message() << ".\n";
              clog << "Resuming coroutine\n";
              asio_handler_invoke(
                  [handler, ec, res]() mutable { handler(ec, res); }, &handler);
              if constexpr (reports_task_end)
                this->handler.report_task_end(chrono::system_clock::now());
            },
            boost::coroutines::attributes{16 << 10});
      clog << "Suspending coroutine\n";
      if constexpr (is_void_v<decltype(completion.result.get())>) {
        completion.result.get();
        clog << "Coroutine resumed\n";
      } else {
        auto r = completion.result.get();
        clog << "Coroutine resumed\n";
        return r;
      }
    }
  }

  void upgrade() {
    spawn(socket.get_io_service(),
          [ this, self = this->shared_from_this(),
            tkt = ticket++ ](yield_context yield) {
            ticket_sentinel sentinel{*this, tkt};
            while (!sentinel)
              socket.get_io_service().post(yield);

            boost::system::error_code ec;
            http::response<http::string_body> res;
            auto remote = socket.remote_endpoint().address().to_string();
            clog << "Upgrading websocket host: " << remote << '\n';
            if constexpr (supports_request_decoration) {
              ws.async_handshake_ex(res, remote, "/",
                                    [this](auto &request) {
                                      ws_stuff.websocket_handler.decorate(
                                          request);
                                    },
                                    yield[ec]);
            } else {
              ws.async_handshake(res, remote, "/", yield[ec]);
            }
            clog << "Thread: " << this_thread::get_id()
                 << "; Connected to websocket: " << ec.message() << '\n';
            if constexpr (reports_upgrade) {
              ws_stuff.open = true;
              handler.report_upgrade(chrono::system_clock::now());
            }
            if constexpr (supports_upgrade_inspection)
              ws_stuff.websocket_handler.inspect(res);
          },
          boost::coroutines::attributes{8 << 10});
  }

public:
  connection(io_service &service, private_construction_tag)
      : socket{service}, ws{socket}, ws_stuff(&buf) {}
  connection() = delete;
  ~connection() {
    if constexpr (reports_close)
      handler.report_close(chrono::system_clock::now(), ws_stuff.open);
    clog << "Connection deleted.\n";
  }
};

template <typename Handler, typename... Args>
void accept(reference_wrapper<io_service> service, Args &&... args) {
  spawn(service.get(), [ service, endpoint = ip::tcp::endpoint(
                                      args...) ](yield_context yield) {
    boost::system::error_code ec;
    ip::tcp::acceptor acceptor{service};
    acceptor.open(endpoint.protocol(), ec);
    if (ec) {
      clog << ec.message() << '\n';
      return;
    }
    acceptor.set_option(socket_base::reuse_address{true});
    acceptor.bind(endpoint, ec);
    if (ec) {
      clog << ec.message() << '\n';
      return;
    }
    acceptor.listen(socket_base::max_connections, ec);
    clog << "Thread: " << this_thread::get_id()
         << "; Start listening for connections: " << ec.message() << '\n';
    if (ec)
      return;
    while (true) {
      auto conn = make_shared<connection<Handler>>(
          service, typename connection<Handler>::private_construction_tag{});
      acceptor.async_accept(conn->socket, yield[ec]);
      clog << "Thread: " << this_thread::get_id()
           << "; Accepted connection: " << ec.message() << '\n';
      if (ec)
        break;
      if constexpr (connection<Handler>::reports_accept)
        conn->handler.report_accept(chrono::system_clock::now());
      spawn(service.get(),
            [conn = move(conn)](yield_context yield) { conn->serve(yield); },
            boost::coroutines::attributes{16 << 10});
    }
  },
        boost::coroutines::attributes{8 << 10});
}

template <typename Handler> class client_connection {
  shared_ptr<connection<Handler>> connection;

  client_connection(decltype(connection) c) : connection(c) {}

  template <typename H, typename... Args>
  friend client_connection<H> connect(reference_wrapper<io_service> service,
                                      Args &&... args);

  template <typename> friend class wsclient_connection;

public:
  client_connection() = delete;
  client_connection(const client_connection &) = delete;
  client_connection(client_connection &&) = default;
  client_connection &operator=(const client_connection &) = delete;
  client_connection &operator=(client_connection &&) = default;

  template <typename T, typename CompletionHandler>
  auto request(T &&t, CompletionHandler &&ch) {
    return connection->request(forward<T>(t), forward<CompletionHandler>(ch));
  }
};

template <typename Handler, typename... Args>
client_connection<Handler> connect(reference_wrapper<io_service> service,
                                   Args &&... args) {
  static_assert(
      connection<Handler>::supports_http_send,
      "Provided handler class does not support sending HTTP requests");

  auto conn = make_shared<connection<Handler>>(
      service, typename connection<Handler>::private_construction_tag{});

  ++conn->ticket;
  spawn(service.get(),
        [ service, conn,
          endpoint = ip::tcp::endpoint(args...) ](yield_context yield) {
          boost::system::error_code ec;
          ip::tcp::resolver resolver{service};
          auto resolved = resolver.async_resolve(endpoint, yield[ec]);
          clog << "Thread: " << this_thread::get_id()
               << "; Resolved address: " << ec.message() << '\n';
          if (ec)
            return;
          auto connected = async_connect(conn->socket, resolved, yield[ec]);
          clog << "Thread: " << this_thread::get_id()
               << "; Connected to endpoint: " << endpoint << " " << ec.message()
               << '\n';
          if (ec)
            return;
          if constexpr (connection<Handler>::reports_connect)
            conn->handler.report_connect(chrono::system_clock::now());
          ++conn->serving;
        },
        boost::coroutines::attributes{8 << 10});

  return {move(conn)};
}

template <typename Handler> class wsclient_connection {
  shared_ptr<connection<Handler>> connection;

  wsclient_connection(client_connection<Handler> &&http)
      : connection(move(http.connection)) {
    connection->upgrade();
  }

  template <typename T>
  friend wsclient_connection<T> upgrade(client_connection<T> &&client);

public:
  wsclient_connection() = delete;
  wsclient_connection(const wsclient_connection &) = delete;
  wsclient_connection(wsclient_connection &&) = default;
  wsclient_connection &operator=(const wsclient_connection &) = delete;
  wsclient_connection &operator=(wsclient_connection &&) = default;
};

template <typename Handler>
wsclient_connection<Handler> upgrade(client_connection<Handler> &&client) {
  return {move(client)};
}

template <typename Handler, typename... Args>
wsclient_connection<Handler> wsconnect(reference_wrapper<io_service> service,
                                       Args &&... args) {
  return upgrade(connect<Handler>(service, forward<Args>(args)...));
}
} // namespace connection_detail

using connection_detail::accept;
using connection_detail::connect;
using connection_detail::upgrade;
using connection_detail::wsconnect;
} // namespace n2w
#endif