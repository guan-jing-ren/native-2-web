#include <string>
#include <iostream>
#include <vector>
#include <tuple>
#include <utility>
#include <numeric>

constexpr const char *reader_template =
    R"(function %%API%%(ws, callback%%ARGS%%) {
  ws.binaryType = "arraybuffer";

  let return_value = [];
  let states = [
%%RET_STATES%%
  ];

  let reader = function(event) {
    states.shift()(event.data);

    if(states.length == 0) {
      ws.removeEventListener("message", reader);
      callback(return_value[0]);
    }
  };

  ws.addEventListener("message", reader);

  let args = [%%ARGS%%];
  let view;

  ws.send("Calling %%REMOTE_FUNC%%()...");

%%WRITERS%%
}

)";

template <typename T> std::string to_ecma;
template <>
std::string to_ecma<unsigned char> = "Uint" +
                                     std::to_string(sizeof(char) * CHAR_BIT);
template <>
std::string to_ecma<unsigned short> = "Uint" +
                                      std::to_string(sizeof(short) * CHAR_BIT);
template <>
std::string to_ecma<unsigned int> = "Uint" +
                                    std::to_string(sizeof(int) * CHAR_BIT);
template <>
std::string to_ecma<char> = "Int" + std::to_string(sizeof(char) * CHAR_BIT);
template <>
std::string to_ecma<short> = "Int" + std::to_string(sizeof(short) * CHAR_BIT);
template <>
std::string to_ecma<int> = "Int" + std::to_string(sizeof(int) * CHAR_BIT);
template <>
std::string to_ecma<float> = "Float" + std::to_string(sizeof(float) * CHAR_BIT);
template <>
std::string to_ecma<double> = "Float" +
                              std::to_string(sizeof(double) * CHAR_BIT);

template <int... I>
std::ostream &create_arg_writer(std::ostream &out,
                                std::integer_sequence<int, I...>, bool arg) {
  auto element = {('[' + std::to_string(I) + ']')...};
  return out << "  ws.send(args"
             << std::accumulate(begin(element), end(element), std::string{})
             << ".toString());" << std::endl
             << std::endl;
}

template <int... I, typename T>
auto create_arg_writer(std::ostream &out, std::integer_sequence<int, I...>,
                       T arg)
    -> std::enable_if_t<std::is_arithmetic<T>::value, std::ostream &> {
  auto element = {('[' + std::to_string(I) + ']')...};
  return out << "  ws.send(args"
             << std::accumulate(begin(element), end(element), std::string{})
             << ".toString());" << std::endl
             << std::endl;
}

template <int... I, typename... Args>
std::ostream &create_arg_writer(std::ostream &out,
                                std::integer_sequence<int, I...>,
                                const std::basic_string<char, Args...> &arg) {
  auto element = {('[' + std::to_string(I) + ']')...};
  out << "  ws.send(args"
      << std::accumulate(begin(element), end(element), std::string{})
      << ".length.toString());" << std::endl;
  return out << "  ws.send(args"
             << std::accumulate(begin(element), end(element), std::string{})
             << ");" << std::endl
             << std::endl;
}

constexpr const char *writer_template =
    R"(  view = new %%VIEWTYPE%%Array(args%%INDICES%%);
  ws.send(view.length.toString());
  ws.send(view.buffer);
)";

template <int... I, typename T, typename... Args>
auto create_arg_writer(std::ostream &out, std::integer_sequence<int, I...>,
                       const std::vector<T, Args...> &arg)
    -> std::enable_if_t<std::is_arithmetic<T>::value, std::ostream &> {
  auto element = {('[' + std::to_string(I) + ']')...};
  auto writer = std::regex_replace(
      writer_template, std::regex{"%%INDICES%%"},
      std::accumulate(begin(element), end(element), std::string{}));
  writer = std::regex_replace(writer, std::regex{"%%VIEWTYPE%%"}, to_ecma<T>);
  return out << writer << std::endl;
}

template <int... C, int... I, typename... Args>
std::ostream &create_arg_writer(std::ostream &out,
                                std::integer_sequence<int, C...> context,
                                const std::tuple<Args...> &arg);

template <int... I, typename T, typename... Args>
auto create_arg_writer(std::ostream &out, std::integer_sequence<int, I...>,
                       const std::vector<T, Args...> &arg)
    -> std::enable_if_t<std::is_class<T>::value, std::ostream &> {
  auto element = {('[' + std::to_string(I) + ']')...};
  auto elements = std::accumulate(begin(element), end(element), std::string{});
  out << "  ws.send(args" << elements << ".length.toString());" << std::endl;
  auto counter = std::string(1, (char)'a' + sizeof...(I)-1);
  out << "  for(let " << counter << " in args" << elements << ") {"
      << std::endl;
  std::ostringstream writer;
  create_arg_writer(
      writer,
      std::integer_sequence<int, I..., -static_cast<int>(sizeof...(I))>{}, T{});
  auto loop = std::regex_replace(
      writer.str(), std::regex(std::to_string(-static_cast<int>(sizeof...(I)))),
      counter);
  return out << loop << std::endl << "  }" << std::endl << std::endl;
}

template <int... C, int... I, typename... Args>
std::ostream &create_arg_writer(std::ostream &out,
                                std::integer_sequence<int, C...>,
                                std::integer_sequence<int, I...>,
                                const std::tuple<Args...> &arg) {
  std::initializer_list<int>{
      (create_arg_writer(out, std::integer_sequence<int, C..., I>{},
                         std::get<I>(arg)),
       0)...};
  return out;
}

template <int... C, int... I, typename... Args>
std::ostream &create_arg_writer(std::ostream &out,
                                std::integer_sequence<int, C...> context,
                                const std::tuple<Args...> &arg) {
  return create_arg_writer(
      out, context, std::make_integer_sequence<int, sizeof...(Args)>{}, arg);
}

constexpr const char *return_template =
    R"(    function(data) {
      return_value%%INDICES%% = data;
    },
)";

template <int... I>
std::ostream &create_return_reader(std::ostream &out,
                                   std::integer_sequence<int, I...>, bool arg) {
  auto element = {('[' + std::to_string(I) + ']')...};
  auto ret = std::regex_replace(
      return_template, std::regex{"%%INDICES%%"},
      std::accumulate(begin(element), end(element), std::string{}));
  ret = std::regex_replace(ret, std::regex{"\\{"}, R"({
      if(data !== "true" && data !== "false")
        throw "Expecting a boolean value string";
  )");
  ret = std::regex_replace(ret, std::regex{"= data;"},
                           R"(= (data === "true" ? true : false);)");
  return out << ret << std::endl;
}

template <int... I, typename T>
auto create_return_reader(std::ostream &out, std::integer_sequence<int, I...>,
                          T arg)
    -> std::enable_if_t<std::is_arithmetic<T>::value, std::ostream &> {
  auto element = {('[' + std::to_string(I) + ']')...};
  auto ret = std::regex_replace(
      return_template, std::regex{"%%INDICES%%"},
      std::accumulate(begin(element), end(element), std::string{}));
  ret = std::regex_replace(ret, std::regex{"= data;"},
                           "= Number.parseFloat(data);");
  return out << ret << std::endl;
}

constexpr const char *receive_length_template =
    R"(    function(data) {
      let length = Number.parseInt(data);
      let top = states.shift();
      states.unshift(top, length);
    },
)";

template <int... I, typename T, typename... Args>
auto create_return_reader(std::ostream &out, std::integer_sequence<int, I...>,
                          const std::vector<T, Args...> &arg)
    -> std::enable_if_t<std::is_arithmetic<T>::value, std::ostream &> {
  out << receive_length_template;

  auto element = {('[' + std::to_string(I) + ']')...};
  auto ret = std::regex_replace(return_template, std::regex{"= data;"},
                                "= Array.prototype.slice.call(new " +
                                    to_ecma<T> + "Array(data));"
                                                 R"(
      let expected = states.shift();
      if(return_value%%INDICES%%.length != expected)
        console.log("Expected " + expected + ", got " + return_value%%INDICES%%.length);
)");
  ret = std::regex_replace(
      ret, std::regex{"%%INDICES%%"},
      std::accumulate(begin(element), end(element), std::string{}));
  return out << ret << std::endl;
}

template <int... I, typename... Args>
std::ostream &
create_return_reader(std::ostream &out, std::integer_sequence<int, I...>,
                     const std::basic_string<char, Args...> &arg) {
  out << receive_length_template;

  auto element = {('[' + std::to_string(I) + ']')...};
  auto ret =
      std::regex_replace(return_template, std::regex{"= data;"}, R"(= data;
      let expected = states.shift();
      if(return_value%%INDICES%%.length != expected)
        console.log("Expected " + expected + ", got " + return_value%%INDICES%%.length);
)");
  ret = std::regex_replace(
      ret, std::regex{"%%INDICES%%"},
      std::accumulate(begin(element), end(element), std::string{}));
  return out << ret << std::endl;
}

template <int... I, typename... Args>
std::ostream &create_return_reader(std::ostream &out,
                                   std::integer_sequence<int, I...> indices,
                                   const std::tuple<Args...> &arg);

template <int... I, typename T, typename... Args>
auto create_return_reader(std::ostream &out, std::integer_sequence<int, I...>,
                          const std::vector<T, Args...> &arg)
    -> std::enable_if_t<std::is_class<T>::value, std::ostream &> {
  auto element = {('[' + std::to_string(I) + ']')...};
  auto elements = std::accumulate(begin(element), end(element), std::string{});
  auto ret = std::regex_replace(return_template, std::regex{"= data;"}, "= [];"
                                                                        R"(
      let %%I%%_length = Number.parseInt(data);
      for(let %%I%% = %%I%%_length - 1; %%I%% >=0; --%%I%%) {
        states = [
%%LOOP%%        ].concat(states);
      }
)");

  ret = std::regex_replace(ret, std::regex{"%%INDICES%%"}, elements);
  auto counter = std::string(1, (char)'a' + sizeof...(I)-1);
  ret = std::regex_replace(ret, std::regex{"%%I%%"}, counter);
  std::ostringstream writer;
  create_return_reader(
      writer,
      std::integer_sequence<int, I..., -static_cast<int>(sizeof...(I))>{}, T{});
  auto loop = std::regex_replace(
      writer.str(), std::regex{std::to_string(-static_cast<int>(sizeof...(I)))},
      counter);
  ret = std::regex_replace(ret, std::regex{"%%LOOP%%"}, loop);
  ret = std::regex_replace(ret, std::regex{",\n\n(\\s+?)\\]"}, "\n$1]");
  return out << ret << std::endl;
}

template <int... C, int... I, typename... Args>
std::ostream &create_return_reader(std::ostream &out,
                                   std::integer_sequence<int, C...>,
                                   std::integer_sequence<int, I...>,
                                   const std::tuple<Args...> &arg) {
  auto element = {('[' + std::to_string(C) + ']')...};
  auto elements = std::accumulate(begin(element), end(element), std::string{});
  auto ret =
      std::regex_replace(return_template, std::regex{"%%INDICES%%"}, elements);
  ret = std::regex_replace(ret, std::regex{"= data;"}, "= [];"
                                                       R"(
      states.shift()(data);
)");
  out << ret << std::endl;
  std::initializer_list<int>{
      (create_return_reader(out, std::integer_sequence<int, C..., I>{},
                            std::get<I>(arg)),
       0)...};
  return out;
}

template <int... I, typename... Args>
std::ostream &create_return_reader(std::ostream &out,
                                   std::integer_sequence<int, I...> indices,
                                   const std::tuple<Args...> &arg) {
  return create_return_reader(
      out, indices, std::make_integer_sequence<int, sizeof...(Args)>{}, arg);
}

std::ostream &create_return_reader(std::ostream &out,
                                   std::integer_sequence<int, 0> indices,
                                   decltype(std::ignore)) {
  return out << R"(    function(data) {
      if(data !== "void")
        throw "Was expecting void function";
      return_value[0] = "void";
})";
}

template <int... I, typename R, typename... Args>
std::ostream &create_reader(std::ostream &out, const std::string &name,
                            std::integer_sequence<int, I...> s,
                            R (*funcs)(Args...)) {
  std::initializer_list<std::string> arg_list = {
      (", arg" + std::to_string(I))...};

  auto reader =
      std::regex_replace(reader_template, std::regex{"%%API%%"}, name);

  std::ostringstream writer;
  create_return_reader(
      writer, std::make_integer_sequence<int, 1>{},
      std::conditional_t<std::is_void<R>::value, decltype(std::ignore), R>());
  reader =
      std::regex_replace(reader, std::regex{"%%RET_STATES%%"}, writer.str());

  auto args = std::accumulate(begin(arg_list), end(arg_list), std::string{});
  reader = std::regex_replace(reader, std::regex{"%%ARGS%%"}, args);
  reader = std::regex_replace(reader, std::regex{"\\[, "}, "[");
  reader = std::regex_replace(
      reader, std::regex{"%%REMOTE_FUNC%%"},
      std::to_string(reinterpret_cast<std::uintptr_t>(funcs)));

  std::ostringstream writers;
  std::initializer_list<int>{
      (create_arg_writer(writers, std::integer_sequence<int, I>{},
                         std::get<I>(std::tuple<std::decay_t<Args>...>{})),
       0)...};

  reader = std::regex_replace(reader, std::regex{"%%WRITERS%%"}, writers.str());
  return out << reader;
}

template <typename R, typename... Args>
void write_api(std::ostream &out, const std::string &name, R (*func)(Args...)) {
  create_reader(out, name, std::make_integer_sequence<int, sizeof...(Args)>{},
                func);
}

#define WRITE_API(out, func) write_api(out, #func, func);
