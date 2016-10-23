thread_local std::array<char, 1 << 12> small_buffer;
thread_local std::vector<char> large_buffer;
thread_local auto last_comm = std::chrono::system_clock::now();
thread_local auto last_data = last_comm;

auto receive_handle_control_frame(Poco::Net::WebSocket &ws, char *data,
                                  int length, int &flags) {
reset_timeout:
  last_comm = std::chrono::system_clock::now();
receive_again:
  try {
    auto read = ws.receiveFrame(data, length, flags);
    if ((Poco::Net::WebSocket::FRAME_OP_PING & flags) ==
        Poco::Net::WebSocket::FRAME_OP_PING) {
      std::cerr << "PING frame received" << std::endl;
      read = ws.sendFrame(data, read, Poco::Net::WebSocket::FRAME_OP_PONG);
      std::cerr << "PONG bytes sent: " << read << std::endl;
      goto reset_timeout;
    } else if ((Poco::Net::WebSocket::FRAME_OP_PONG & flags) ==
               Poco::Net::WebSocket::FRAME_OP_PONG) {
      std::cerr << "PONG frame received" << std::endl;
      std::cerr << "PONG frame data: " << std::string{data, data + read}
                << std::endl;
      goto reset_timeout;
    } else if ((Poco::Net::WebSocket::FRAME_OP_CLOSE & flags) ==
               Poco::Net::WebSocket::FRAME_OP_CLOSE) {
      throw std::string{"CLOSE frame received"};
    }

    last_data = last_comm;

    std::cerr << "Bytes read: " << read << std::endl;
    std::cerr << "Flags: " << std::hex << flags << std::dec << std::endl;
    if (read == 0)
      throw std::string{"Socket unexpectedly closed"};
    if (read < 0)
      throw std::string{"Received negative bytes"};

    return read;
  } catch (const Poco::TimeoutException &e) {
    auto new_comm = std::chrono::system_clock::now();
    auto span = new_comm - last_comm;
    if (span > std::chrono::seconds{60}) {
      std::cerr << "No packets sent in the last "
                << std::chrono::duration_cast<std::chrono::seconds>(span)
                       .count()
                << " seconds" << std::endl;
      auto sent = ws.sendFrame("", 0, Poco::Net::WebSocket::FRAME_OP_CLOSE |
                                          Poco::Net::WebSocket::FRAME_FLAG_FIN);
      std::cerr << "CLOSE bytes sent: " << sent << std::endl;
      std::cerr << "Number of threads "
                << Poco::ThreadPool::defaultPool().used() << std::endl;
      throw e;
    }
    if (new_comm - last_data > std::chrono::seconds{180}) {
      std::cerr << "No data packets sent in the last "
                << std::chrono::duration_cast<std::chrono::seconds>(new_comm -
                                                                    last_data)
                       .count()
                << " seconds" << std::endl;
      auto sent = ws.sendFrame("", 0, Poco::Net::WebSocket::FRAME_OP_CLOSE |
                                          Poco::Net::WebSocket::FRAME_FLAG_FIN);
      std::cerr << "CLOSE bytes sent: " << sent << std::endl;
      std::cerr << "Number of threads "
                << Poco::ThreadPool::defaultPool().used() << std::endl;
      throw e;
    }
    if (span > std::chrono::seconds{30}) {
      auto sent =
          ws.sendFrame("PING", 4, Poco::Net::WebSocket::FRAME_OP_PING |
                                      Poco::Net::WebSocket::FRAME_FLAG_FIN);
      std::cerr << "PING bytes sent: " << sent << std::endl;
      std::cerr << "Number of threads "
                << Poco::ThreadPool::defaultPool().used() << std::endl;
    }
    goto receive_again;
  }
}

void receive(Poco::Net::WebSocket &ws, bool &arg) {
  std::cerr << "Receiving bool" << std::endl;
  int flags;
  auto read = receive_handle_control_frame(ws, begin(small_buffer),
                                           small_buffer.size(), flags);
  if (!(flags & Poco::Net::WebSocket::FRAME_OP_TEXT))
    throw std::string{"Expecting a text frame"};
  if (!(flags & Poco::Net::WebSocket::FRAME_FLAG_FIN))
    throw std::string{"Expecting a complete frame"};
  std::cerr << "Received value: "
            << std::string{begin(small_buffer), begin(small_buffer) + read}
            << std::endl;
  if (std::regex_match(begin(small_buffer), begin(small_buffer) + read,
                       std::regex{"true"})) {
    arg = true;
    return;
  } else if (std::regex_match(begin(small_buffer), begin(small_buffer) + read,
                              std::regex{"false"})) {
    arg = false;
    return;
  }
  throw std::string{"Expected boolean value string"};
}

template <typename T>
auto receive(Poco::Net::WebSocket &ws, T &arg)
    -> std::enable_if_t<std::is_arithmetic<T>::value> {
  std::cerr << "Receiving " << to_ecma<T> << std::endl;
  int flags;
  auto read = receive_handle_control_frame(ws, begin(small_buffer),
                                           small_buffer.size(), flags);
  if (!(flags & Poco::Net::WebSocket::FRAME_OP_TEXT))
    throw std::string{"Expecting a text frame"};
  if (!(flags & Poco::Net::WebSocket::FRAME_FLAG_FIN))
    throw std::string{"Expecting a complete frame"};
  std::size_t pos = 0;
  std::string str{begin(small_buffer), begin(small_buffer) + read};
  std::cerr << "Received value: " << str << std::endl;
  if (std::is_integral<T>::value)
    if (std::is_unsigned<T>::value)
      arg = static_cast<T>(std::stoull(str, &pos));
    else
      arg = static_cast<T>(std::stoll(str, &pos));
  else if (std::is_floating_point<T>::value)
    arg = static_cast<T>(std::stold(str, &pos));

  if (pos != static_cast<std::size_t>(read))
    throw std::string{"Not all characters processed in reading number, may "
                      "contain invalid characters: "};
}

template <typename... Args>
void receive(Poco::Net::WebSocket &ws, std::basic_string<char, Args...> &arg) {
  std::cerr << "Receiving UTF8 string" << std::endl;
  auto size = arg.size();
  receive(ws, size);
  std::cerr << "Receiving string of length: " << size << std::endl;
  arg.reserve(size);
  large_buffer.resize(size);
  arg.clear();
  int flags;
  while (size != 0) {
    auto read =
        receive_handle_control_frame(ws, large_buffer.data(), size, flags);
    if (!(flags & Poco::Net::WebSocket::FRAME_OP_TEXT))
      throw std::string{"Expecting text frame"};
    if (static_cast<std::size_t>(read) > size)
      throw std::string{"Data in frame more than was expected"};
    size -= read;
    std::cerr << "Bytes left: " << size << std::endl;
    if (flags & Poco::Net::WebSocket::FRAME_FLAG_FIN && size > 0)
      throw std::string{
          "Received final frame but with fewer characters than expected"};
    std::copy_n(begin(large_buffer), read, std::back_inserter(arg));
    std::cerr << "Received value: " << arg << std::endl;
  }
}

template <typename T, typename... Args>
auto receive(Poco::Net::WebSocket &ws, std::vector<T, Args...> &arg)
    -> std::enable_if_t<std::is_arithmetic<T>::value> {
  std::cerr << "Receiving " << to_ecma<T> << " vector" << std::endl;
  auto size = arg.size();
  receive(ws, size);
  std::cerr << "Receiving vector of length: " << size << std::endl;
  arg.resize(size);
  size *= sizeof(T);
  auto data = small_buffer.data();
  if (size > small_buffer.size()) {
    if (size > large_buffer.size())
      large_buffer.resize(size);
    data = large_buffer.data();
  }
  arg.clear();
  int flags;
  while (size != 0) {
    auto read = receive_handle_control_frame(ws, data, size, flags);
    if (!(flags & Poco::Net::WebSocket::FRAME_OP_BINARY))
      throw std::string{"Expecting binary frame"};
    if (static_cast<std::size_t>(read) > size)
      throw std::string{"Data in frame more than was expected"};
    if (flags & Poco::Net::WebSocket::FRAME_FLAG_FIN &&
        size > static_cast<std::size_t>(read))
      throw std::string{
          "Received final frame but with fewer characters than expected"};
    std::copy_n(data, read,
                reinterpret_cast<char *>(arg.data() + arg.size()) - size);
    size -= read;
    std::cerr << "Bytes left: " << size << std::endl;
  }
}

template <std::size_t... I, typename... T>
void receive(Poco::Net::WebSocket &ws, std::tuple<T...> &arg);

template <typename T, typename... Args>
auto receive(Poco::Net::WebSocket &ws, std::vector<T, Args...> &arg)
    -> std::enable_if_t<std::is_class<T>::value> {
  std::cerr << "Receiving a vector of objects" << std::endl;
  auto size = arg.size();
  receive(ws, size);
  std::cerr << "Receiving a vector of length: " << size << std::endl;
  arg.resize(size);
  std::for_each(begin(arg), end(arg), [&ws](auto &a) { receive(ws, a); });
}

template <std::size_t... I, typename... T>
void receive(Poco::Net::WebSocket &ws, std::integer_sequence<std::size_t, I...>,
             std::tuple<T...> &arg) {
  std::initializer_list<int>{(receive(ws, std::get<I>(arg)), 0)...};
}

template <std::size_t... I, typename... T>
void receive(Poco::Net::WebSocket &ws, std::tuple<T...> &arg) {
  std::cerr << "Receiving a tuple with " << sizeof...(T) << " elements"
            << std::endl;
  receive(ws, std::make_index_sequence<sizeof...(T)>{}, arg);
}

void respond(Poco::Net::WebSocket &ws, decltype(std::ignore)) {
  std::cerr << "Void response" << std::endl;
  static constexpr const char *ign = "void";
  static constexpr const auto ign_len = std::strlen(ign);
  auto sent = ws.sendFrame(ign, ign_len, Poco::Net::WebSocket::FRAME_TEXT);
  std::cerr << "Bytes sent: " << sent << " out of " << ign_len << std::endl;
}

void respond(Poco::Net::WebSocket &ws, const bool arg) {
  std::cerr << "Bool response" << std::endl;
  static constexpr const char *pos = "true";
  static constexpr const auto pos_len = std::strlen(pos);
  static constexpr const char *neg = "false";
  static constexpr const auto neg_len = std::strlen(neg);
  auto sent = ws.sendFrame(arg ? pos : neg, arg ? pos_len : neg_len,
                           Poco::Net::WebSocket::FRAME_TEXT);
  std::cerr << "Bytes sent: " << sent << " out of " << (arg ? pos_len : neg_len)
            << std::endl;
}

template <typename T>
auto respond(Poco::Net::WebSocket &ws, const T arg)
    -> std::enable_if_t<std::is_arithmetic<T>::value> {
  std::cerr << to_ecma<T> << " response" << std::endl;
  auto value = std::to_string(arg);
  auto sent = ws.sendFrame(value.data(), value.size(),
                           Poco::Net::WebSocket::FRAME_TEXT);
  std::cerr << "Bytes sent: " << sent << " out of " << value.size()
            << std::endl;
}

template <typename... Args>
void respond(Poco::Net::WebSocket &ws,
             const std::basic_string<char, Args...> &arg) {
  std::cerr << "String response" << std::endl;
  respond(ws, arg.size());
  auto sent =
      ws.sendFrame(arg.data(), arg.size(), Poco::Net::WebSocket::FRAME_TEXT);
  std::cerr << "Bytes sent: " << sent << " out of " << arg.size() << std::endl;
}

template <typename T, typename... Args>
auto respond(Poco::Net::WebSocket &ws, const std::vector<T, Args...> &arg)
    -> std::enable_if_t<std::is_arithmetic<T>::value> {
  std::cerr << to_ecma<T> << " vector response" << std::endl;
  respond(ws, arg.size());
  auto sent = ws.sendFrame(arg.data(), arg.size() * sizeof(T),
                           Poco::Net::WebSocket::FRAME_BINARY);
  std::cerr << "Bytes sent: " << sent << " out of " << (arg.size() * sizeof(T))
            << std::endl;
}

template <std::size_t... I, typename... T>
void respond(Poco::Net::WebSocket &ws, const std::tuple<T...> &arg);

template <typename T, typename... Args>
auto respond(Poco::Net::WebSocket &ws, const std::vector<T, Args...> &arg)
    -> std::enable_if_t<std::is_class<T>::value> {
  std::cerr << "Vector of objects response" << std::endl;
  std::cerr << "Number of objects in vector: " << arg.size() << std::endl;
  respond(ws, arg.size());
  std::for_each(begin(arg), end(arg), [&ws](const auto &a) { respond(ws, a); });
}

template <std::size_t... I, typename... T>
void respond(Poco::Net::WebSocket &ws, std::integer_sequence<std::size_t, I...>,
             const std::tuple<T...> &arg) {
  std::initializer_list<int>{(respond(ws, std::get<I>(arg)), 0)...};
}

template <std::size_t... I, typename... T>
void respond(Poco::Net::WebSocket &ws, const std::tuple<T...> &arg) {
  std::cerr << "Tuple response" << std::endl;
  std::cerr << "Tuple containing " << sizeof...(T) << " elements" << std::endl;
  respond(ws, std::make_index_sequence<sizeof...(T)>{}, arg);
}

template <std::size_t... I, typename R, typename... Args>
auto invoke(R (*func)(Args...), std::integer_sequence<std::size_t, I...>,
            const std::tuple<std::decay_t<Args>...> &args)
    -> std::enable_if_t<!std::is_void<R>::value, R> {
  std::cerr << "Invoking result function" << std::endl;
  return func(std::get<I>(args)...);
}

template <std::size_t... I, typename... Args>
decltype(std::ignore) invoke(void (*func)(Args...),
                             std::integer_sequence<std::size_t, I...>,
                             const std::tuple<std::decay_t<Args>...> &args) {
  std::cerr << "Invoking void function" << std::endl;
  func(std::get<I>(args)...);
  return std::ignore;
}

template <typename R, typename... Args>
void invoke(Poco::Net::WebSocket &ws, R (*func)(Args...)) {
  std::cerr << "Handling rpc: " << reinterpret_cast<std::uintptr_t>(func)
            << std::endl;
  std::tuple<std::decay_t<Args>...> args;
  receive(ws, std::make_index_sequence<sizeof...(Args)>{}, args);
  respond(ws, invoke(func, std::make_index_sequence<sizeof...(Args)>{}, args));
  std::cerr << reinterpret_cast<std::uintptr_t>(func) << " rpc handled"
            << std::endl;
}

using rpc = void (*)();
using rpc_registry = const rpc[];

template <typename...> struct web_invoker {};

template <typename R, typename... Args> struct web_invoker<R(Args...)> {
  static void call(Poco::Net::WebSocket &ws, rpc func) {
    invoke(ws, reinterpret_cast<R (*)(Args...)>(func));
  }
};

void no_op(Poco::Net::WebSocket &, rpc) {}

#define DEFINE_RPC_REGISTER(f) constexpr rpc_registry f = {
#define REGISTER_RPC(f)                                                        \
  reinterpret_cast<rpc>(web_invoker<decltype(f)>::call),                       \
      reinterpret_cast<rpc>(f)
#define FINALIZE_RPC_REGISTER                                                  \
  reinterpret_cast<rpc>(no_op)                                                 \
  }                                                                            \
  ;

template <typename Registry>
auto get_rpc_handler(const Registry &registry, rpc func) {
  auto f = std::find(std::begin(registry) + 1, std::end(registry), func) - 1;
  std::cerr << "Found: " << f << ", end: " << std::end(registry) << std::endl;
  return
      [ f = reinterpret_cast<void (*)(Poco::Net::WebSocket &, rpc)>(*f),
        func ](Poco::Net::WebSocket & ws) {
    f(ws, func);
  };
}
