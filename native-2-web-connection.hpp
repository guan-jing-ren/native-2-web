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
#define BOOST_COROUTINES_UNIDIRECT
#include <boost/asio/spawn.hpp>

#include <thread>

template <typename> class n2w_client_connection;

template <typename Handler>
class n2w_connection final
    : public std::enable_shared_from_this<n2w_connection<Handler>> {

  /***************************/
  /* HTTP SFINAE DEFINITIONS */
  /***************************/

  static auto http_supports_receive(...) -> std::false_type;
  template <typename T = Handler>
  static auto http_supports_receive(T *)
      -> std::result_of_t<T(beast::http::request<beast::http::string_body>)>;
  static constexpr bool supports_http_receive =
      !std::is_same_v<decltype(
                          http_supports_receive(std::declval<Handler *>())),
                      std::false_type>;

  struct adaptable;

  static auto http_supports_send(...) -> std::false_type;
  template <typename T = Handler>
  static auto http_supports_send(T *) -> std::result_of_t<T(adaptable)>;
  static constexpr bool supports_http_send =
      std::is_same_v<decltype(http_supports_send(std::declval<Handler *>())),
                     beast::http::request<beast::http::string_body>>;

  static constexpr bool supports_http =
      supports_http_receive || supports_http_send;

  /**************************************/
  /* BASIC WEBSOCKET SFINAE DEFINITIONS */
  /**************************************/

  static auto websocket_support(...) -> std::false_type;
  template <typename T = Handler>
  static auto websocket_support(T *) -> typename T::websocket_handler_type;
  using websocket_handler_type =
      decltype(websocket_support(std::declval<Handler *>()));
  static constexpr bool supports_websocket =
      !std::is_same_v<websocket_handler_type, std::false_type>;

  template <typename> static auto websocket_handles(...) -> std::false_type;
  template <typename V, typename T = Handler>
  static auto websocket_handles(T *)
      -> std::result_of_t<typename T::websocket_handler_type(V)>;
  static constexpr bool websocket_handles_text =
      supports_websocket &&
      !std::is_same_v<decltype(websocket_handles<std::string>(
                          std::declval<Handler *>())),
                      std::false_type>;
  static constexpr bool websocket_handles_binary =
      supports_websocket &&
      !std::is_same_v<decltype(websocket_handles<std::vector<uint8_t>>(
                          std::declval<Handler *>())),
                      std::false_type>;

  /*****************************************/
  /* EXTENDED WEBSOCKET SFINAE DEFINITIONS */
  /*****************************************/

  template <typename> static auto websocket_pushes(...) -> std::false_type;
  template <typename V, typename T = Handler>
  static auto websocket_pushes(T *) -> std::result_of_t<
      typename T::websocket_handler_type(std::function<void(V)>)>;
  static constexpr bool websocket_pushes_text =
      supports_websocket &&
      !std::is_same_v<decltype(websocket_pushes<std::string>(
                          std::declval<Handler *>())),
                      std::false_type>;
  static constexpr bool websocket_pushes_binary =
      supports_websocket &&
      !std::is_same_v<decltype(websocket_pushes<std::vector<uint8_t>>(
                          std::declval<Handler *>())),
                      std::false_type>;

  /**********************************/
  /* INTERNAL STRUCTURE DEFINITIONS */
  /**********************************/

  struct adaptable {
    template <typename T> operator T();
  };

  struct NullHandler {
    beast::http::response<beast::http::string_body>
    operator()(const beast::http::request<beast::http::string_body> &request) {
      auto response = beast::http::response<beast::http::string_body>{};
      response.version = request.version;
      response.result(501);
      response.reason("Not Implemented");
      response.body = "n2w server does implement websocket handling";
      return response;
    }
  };

  struct websocket_stuff {
    websocket_handler_type websocket_handler;
    std::istream stream;
    websocket_stuff(boost::asio::streambuf *buf) : stream(buf) {}
  };

  /*******************/
  /* INTERNAL LAYOUT */
  /*******************/

  boost::asio::ip::tcp::socket socket;
  beast::websocket::stream<boost::asio::ip::tcp::socket &> ws;
  std::atomic_uintmax_t ticket{0};
  std::atomic_uintmax_t serving{0};

  boost::asio::streambuf buf;
  std::conditional_t<supports_http, Handler, NullHandler> handler;
  std::conditional_t<supports_websocket, websocket_stuff, void *> ws_stuff{
      &buf};

  /***********************/
  /* INTERNAL OPERATIONS */
  /***********************/

  template <typename R>
  void write_response(boost::asio::yield_context yield, R reply,
                      std::uintmax_t tkt) {
    boost::system::error_code ec;
    while (tkt != serving) {
      std::clog << "Waiting to serve: " << tkt
                << ", currently serving: " << serving
                << ", is open: " << std::boolalpha << socket.is_open() << '\n';
      socket.get_io_service().post(yield[ec]);
    }

    std::string response_type;
    if
      constexpr(
          std::is_same_v<R, beast::http::response<beast::http::string_body>>) {
        reply.prepare_payload();
        beast::http::async_write(socket, reply, yield[ec]);
        response_type = "HTTP";
      }
    else if
      constexpr(!std::is_same_v<R, nullptr_t> && supports_websocket) {
        if
          constexpr(std::is_same_v<R, std::string>) {
            ws.text(true);
            response_type = "text websocket";
          }
        else if
          constexpr(std::is_same_v<R, std::vector<uint8_t>>) {
            ws.binary(true);
            response_type = "binary_websocket";
          }
        ws.async_write(boost::asio::buffer(reply), yield[ec]);
        if (ec)
          ws_stuff.websocket_handler = websocket_handler_type{};
      }

    std::clog << "Thread: " << std::this_thread::get_id() << "; Written "
              << response_type << " response:" << ec << ".\n";
    ++serving;
  }

  template <typename T> auto async(T t) {
    if
      constexpr(
          std::is_same_v<T, beast::http::response<beast::http::string_body>> ||
          std::is_same_v<T, std::string> ||
          std::is_same_v<T, std::vector<uint8_t>>) {
        boost::asio::spawn(socket.get_io_service(), [
          this, self = this->shared_from_this(), t = std::forward<T>(t),
          tkt = ticket++
        ](boost::asio::yield_context yield) {
          write_response(yield, move(t), tkt);
        });
      }
    else {
      boost::asio::spawn(socket.get_io_service(), [
        this, self = this->shared_from_this(), t = std::forward<T>(t),
        tkt = ticket++
      ](boost::asio::yield_context yield) {
        if
          constexpr(std::is_void_v<std::result_of_t<T()>>) {
            t();
            write_response(yield, nullptr, tkt);
          }
        else
          write_response(yield, t(), tkt);
      });
    }
  }

  void serve(boost::asio::yield_context yield) {
    socket.set_option(boost::asio::ip::tcp::no_delay{true});

    boost::system::error_code ec;
    while (true) {
      beast::http::request<beast::http::string_body> request;
      beast::http::async_read(socket, buf, request, yield[ec]);
      std::clog << "Thread: " << std::this_thread::get_id()
                << "; Received request: " << ec << ".\n";
      if (ec)
        break;

      if (beast::websocket::is_upgrade(request)) {
        if
          constexpr(supports_websocket) {
            ws.async_accept(request, yield[ec]);
            std::clog << "Accepted websocket connection: " << ec << '\n';
            if (ec)
              break;
            ws_stuff.stream >> std::noskipws;
            register_websocket_pusher();
            goto do_upgrade;
          }
        else
          async(NullHandler{}(request));
        return;
      }

      if
        constexpr(supports_http_receive) async(
            [ this, request = move(request) ]() { return handler(request); });
      else
        break;
    }
    std::clog << "Finished serving socket\n";

    return;
  do_upgrade:
    ws_serve(ec, yield);
    std::clog << "Finished serving websocket\n";
  }

  void ws_serve(boost::system::error_code ec,
                boost::asio::yield_context yield) {
    if
      constexpr(supports_websocket) {
        auto save_stream = [this](auto &message) {
          std::copy(std::istream_iterator<char>(ws_stuff.stream), {},
                    back_inserter(message));
          ws_stuff.stream.clear();
        };

        while (true) {
          ws.async_read(buf, yield[ec]);
          std::clog << "Read something from websocket: " << ec << '\n';
          if (ec)
            break;
          if (ws.got_text()) {
            std::string message;
            save_stream(message);
            std::clog << "Text message received: " << message << '\n';
            if
              constexpr(websocket_handles_text)
                  async([ this, message = move(message) ] {
                    return ws_stuff.websocket_handler(message);
                  });
          } else if (ws.got_binary()) {
            std::vector<uint8_t> message;
            save_stream(message);
            std::clog << "Binary message received, size: " << message.size()
                      << '\n';
            if
              constexpr(websocket_handles_binary)
                  async([ this, message = move(message) ] {
                    return ws_stuff.websocket_handler(message);
                  });
          } else {
            std::clog << "Unsupported WebSocket opcode: " << '\n';
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

  template <typename, typename... Args>
  friend void accept(std::reference_wrapper<boost::asio::io_service> service,
                     Args &&... args);

  template <typename> friend class n2w_client_connection;

  template <typename H, typename... Args>
  friend n2w_client_connection<H>
  connect(std::reference_wrapper<boost::asio::io_service> service,
          Args &&... args);

  struct private_construction_tag {};

public:
  n2w_connection(boost::asio::io_service &service, private_construction_tag)
      : socket{service}, ws{socket}, ws_stuff(&buf) {}
  n2w_connection() = delete;
};

template <typename Handler, typename... Args>
void accept(std::reference_wrapper<boost::asio::io_service> service,
            Args &&... args) {
  static_assert(n2w_connection<Handler>::supports_http_receive ||
                    n2w_connection<Handler>::supports_websocket,
                "Provided handler class does not support sending HTTP or "
                "Websocket responses.");

  boost::asio::spawn(service.get(), [
    service, endpoint = boost::asio::ip::tcp::endpoint(args...)
  ](boost::asio::yield_context yield) {
    boost::system::error_code ec;
    boost::asio::ip::tcp::acceptor acceptor{service, endpoint, true};
    acceptor.listen(boost::asio::socket_base::max_connections, ec);
    std::clog << "Thread: " << std::this_thread::get_id()
              << "; Start listening for connections: " << ec << '\n';
    if (ec)
      return;
    while (true) {
      auto connection = std::make_shared<n2w_connection<Handler>>(
          service,
          typename n2w_connection<Handler>::private_construction_tag{});
      acceptor.async_accept(connection->socket, yield[ec]);
      std::clog << "Thread: " << std::this_thread::get_id()
                << "; Accepted connection: " << ec << '\n';
      if (ec)
        break;
      boost::asio::spawn(service.get(), [connection = move(connection)](
                                            boost::asio::yield_context yield) {
        connection->serve(yield);
      });
    }
  });
}

template <typename Handler> class n2w_client_connection {
  std::shared_ptr<n2w_connection<Handler>> connection;

  n2w_client_connection(decltype(connection) c) : connection(c) {}

  template <typename H, typename... Args>
  friend n2w_client_connection<H>
  connect(std::reference_wrapper<boost::asio::io_service> service,
          Args &&... args);

public:
  n2w_client_connection() = delete;
  n2w_client_connection(const n2w_client_connection &) = delete;
  n2w_client_connection(n2w_client_connection &&) = default;
  n2w_client_connection &operator=(const n2w_client_connection &) = delete;
  n2w_client_connection &operator=(n2w_client_connection &&) = default;
};

template <typename Handler, typename... Args>
n2w_client_connection<Handler>
connect(std::reference_wrapper<boost::asio::io_service> service,
        Args &&... args) {
  static_assert(
      n2w_connection<Handler>::supports_http_send,
      "Provided handler class does not support sending HTTP requests");

  auto connection = std::make_shared<n2w_connection<Handler>>(
      service, typename n2w_connection<Handler>::private_construction_tag{});

  boost::asio::spawn(service.get(), [
    service, connection, endpoint = boost::asio::ip::tcp::endpoint(args...)
  ](boost::asio::yield_context yield) {
    boost::system::error_code ec;
    boost::asio::ip::tcp::resolver resolver{service};
    auto resolved = resolver.async_resolve(endpoint, yield[ec]);
    std::clog << "Thread: " << std::this_thread::get_id()
              << "; Resolved address: " << ec << '\n';
    if (ec)
      return;
    auto connected =
        boost::asio::async_connect(connection->socket, resolved, yield[ec]);
    std::clog << "Thread: " << std::this_thread::get_id()
              << "; Connected to endpoint: " << ec << '\n';
    if (ec)
      return;
  });

  return {std::move(connection)};
}

#endif