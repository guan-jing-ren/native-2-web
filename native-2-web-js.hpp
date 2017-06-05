#ifndef _NATIVE_2_WEB_JS_HPP_
#define _NATIVE_2_WEB_JS_HPP_

#include "native-2-web-common.hpp"

namespace n2w {
using namespace std;

template <typename T> string js_constructor = "";
template <> const string js_constructor<bool> = "Uint8";
template <> const string js_constructor<char> = "Int8";
template <> const string js_constructor<wchar_t> = "Uint32";
template <> const string js_constructor<char16_t> = "Uint16";
template <> const string js_constructor<char32_t> = "Uint32";
template <> const string js_constructor<int8_t> = "Int8";
template <> const string js_constructor<int16_t> = "Int16";
template <> const string js_constructor<int32_t> = "Int32";
template <> const string js_constructor<uint8_t> = "Uint8";
template <> const string js_constructor<uint16_t> = "Uint16";
template <> const string js_constructor<uint32_t> = "Uint32";
template <> const string js_constructor<float> = "Float32";
template <> const string js_constructor<double> = "Float64";

template <typename T> struct to_js {
  template <typename U = T>
  static auto create_reader() -> enable_if_t<is_arithmetic<U>{}, string> {
    return is_same<U, bool>{} ? R"(read_bool)"
                              :
                              R"(function (data, offset) {
  return read_number(data, offset, 'get)" +
                                  js_constructor<U> + R"(');
})";
  }
  template <typename U = T>
  static auto create_writer() -> enable_if_t<is_arithmetic<U>{}, string> {
    return is_same<U, bool>{} ? R"(write_bool)"
                              :
                              R"(function (object) {
  return write_number(object, 'set)" +
                                  js_constructor<U> + R"(');
})";
  }
  template <typename U = T>
  static auto create_html() -> enable_if_t<is_arithmetic<U>{}, string> {
    return is_same<U, bool>{} ? R"(this.html_bool)" : R"(this.html_number)";
  }
  template <typename U = T>
  static auto create_reader() -> enable_if_t<is_enum<U>{}, string> {
    return R"(function (data, offset) {
  return read_enum(data, offset, 'get)" +
           js_constructor<underlying_type_t<U>> + R"(');
})";
  }
  template <typename U = T>
  static auto create_writer() -> enable_if_t<is_enum<U>{}, string> {
    return R"(function (object) {
  return write_enum(object, 'set)" +
           js_constructor<underlying_type_t<U>> + R"(');
})";
  }
  template <typename U = T>
  static auto create_html() -> enable_if_t<is_enum<U>{}, string> {
    return R"(function (parent, value, dispatcher) {
  return this.html_enum(parent, value, dispatcher, { )" +
           accumulate(
               cbegin(enumeration<U>::e_to_str), cend(enumeration<U>::e_to_str),
               string{},
               [](const auto &enums, const auto &en) {
                 return enums + (enums.empty() ? "" : ", ") + "'" +
                        to_string(static_cast<underlying_type_t<U>>(en.first)) +
                        "': '" + en.second + "'";
               }) +
           R"( });
})";
  }
};

template <> struct to_js<void *> {
  static string create_reader() { return "function (){}"; }
  static string create_writer() { return "function (){}"; }
  static string create_html() { return "function (){}"; }
};

template <> struct to_js<char> {
  static string create_reader() { return R"(read_char)"; }
  static string create_writer() { return R"(write_char)"; }
  static string create_html() { return R"(this.html_char)"; }
};
template <> struct to_js<char32_t> {
  static string create_reader() { return R"(read_char32)"; }
  static string create_writer() { return R"(write_char32)"; }
  static string create_html() { return R"(this.html_char32)"; }
};

template <> struct to_js<char16_t> : to_js<char32_t> {};
template <> struct to_js<wchar_t> : to_js<char32_t> {};

template <typename T, typename... Ts> struct to_js_heterogenous {
  static string create_reader() {
    return to_js<T>::create_reader() + ",\n" +
           to_js_heterogenous<Ts...>::create_reader();
  }
  static string create_writer() {
    return to_js<T>::create_writer() + ",\n" +
           to_js_heterogenous<Ts...>::create_writer();
  }
  static string create_html() {
    return to_js<T>::create_html() + ",\n" +
           to_js_heterogenous<Ts...>::create_html();
  }
};

template <typename T> struct to_js_heterogenous<T> {
  static string create_reader() { return to_js<T>::create_reader(); }
  static string create_writer() { return to_js<T>::create_writer(); }
  static string create_html() { return to_js<T>::create_html(); }
};

template <typename T, typename U> struct to_js<pair<T, U>> {
  static string create_reader() {
    return R"(function (data, offset) {
  return read_structure(data, offset, [)" +
           to_js_heterogenous<T, U>::create_reader() +
           R"(], ['first', 'second']);
})";
  }
  static string create_writer() {
    return R"(function (object) {
  return write_structure(object, [)" +
           to_js_heterogenous<T, U>::create_writer() +
           R"(], ['first', 'second']);
})";
  }
  static string create_html() {
    return R"(function (parent, value, dispatcher) {
  return this.html_structure(parent, value, dispatcher, [)" +
           to_js_heterogenous<T, U>::create_html() + R"(], ['first', 'second']);
})";
  }
};

template <typename T, typename... Ts> struct to_js<tuple<T, Ts...>> {
  static string create_reader() {
    return R"(function (data, offset, names, base_readers, base_names) {
  return read_structure(data, offset, [)" +
           to_js_heterogenous<T, Ts...>::create_reader() +
           R"(], names, base_readers, base_names);
})";
  }
  static string create_writer() {
    return R"(function (object, names, base_writers, base_names) {
  return write_structure(object, [)" +
           to_js_heterogenous<T, Ts...>::create_writer() +
           R"(], names, base_writers, base_names);
})";
  }
  static string create_html() {
    return R"(function (parent, value, dispatcher, names, base_html, base_names) {
  return this.html_structure(parent, value, dispatcher, [)" +
           to_js_heterogenous<T, Ts...>::create_html() +
           R"(], names, base_html, base_names);
})";
  }
};

template <typename T, typename... Traits>
struct to_js<basic_string<T, Traits...>> {
  static string create_reader() { return "read_string"; }
  static string create_writer() { return "write_string"; }
  static string create_html() { return "this.html_string"; }
};

template <typename T, typename... Traits> struct to_js<vector<T, Traits...>> {
  template <typename U = T>
  static auto create_reader() -> enable_if_t<is_arithmetic<U>{}, string> {
    return R"(function (data, offset) {
  return read_numbers(data, offset, 'get)" +
           js_constructor<U> + R"(');
})";
  }

  template <typename U = T>
  static auto create_reader()
      -> enable_if_t<is_class<U>{} || is_enum<U>{}, string> {
    return R"(function (data, offset) {
  return read_structures(data, offset, )" +
           to_js<U>::create_reader() + R"();
})";
  }

  template <typename U = T>
  static auto create_writer() -> enable_if_t<is_arithmetic<U>{}, string> {
    return R"(function (object) {
  return write_numbers(object, 'set)" +
           js_constructor<U> + R"(');
})";
  }

  template <typename U = T>
  static auto create_writer()
      -> enable_if_t<is_class<U>{} || is_enum<U>{}, string> {
    return R"(function (object) {
  return write_structures(object, )" +
           to_js<U>::create_writer() + R"();
})";
  }

  static auto create_html() {
    return R"(function (parent, value, dispatcher) {
  return this.html_sequence(parent, value, dispatcher, )" +
           to_js<T>::create_html() + R"();
})";
  }
};

template <typename T, typename... Traits>
struct to_js<list<T, Traits...>> : to_js<vector<T>> {};
template <typename T, typename... Traits>
struct to_js<forward_list<T, Traits...>> : to_js<vector<T>> {};
template <typename T, typename... Traits>
struct to_js<deque<T, Traits...>> : to_js<vector<T>> {};
template <typename T, typename... Traits>
struct to_js<set<T, Traits...>> : to_js<vector<T>> {};
template <typename T, typename... Traits>
struct to_js<multiset<T, Traits...>> : to_js<vector<T>> {};
template <typename T, typename... Traits>
struct to_js<unordered_set<T, Traits...>> : to_js<vector<T>> {};
template <typename T, typename... Traits>
struct to_js<unordered_multiset<T, Traits...>> : to_js<vector<T>> {};

template <typename T> struct to_js_bounded {
  template <typename U = T>
  static auto create_reader() -> enable_if_t<is_arithmetic<U>{}, string> {
    return R"(function (data, offset, size) {
  return read_numbers_bounded(data, offset, 'get)" +
           js_constructor<U> + R"(', size);
})";
  }
  template <typename U = T>
  static auto create_reader()
      -> enable_if_t<is_class<U>{} || is_enum<U>{}, string> {
    return R"(function (data, offset, size) {
  return read_structures_bounded(data, offset, )" +
           to_js<U>::create_reader() + R"(, size);
})";
  }

  template <typename U = T>
  static auto create_writer() -> enable_if_t<is_arithmetic<U>{}, string> {
    return R"(function (object, size) {
  return write_numbers_bounded(object, 'set)" +
           js_constructor<U> + R"(', size);
})";
  }
  template <typename U = T>
  static auto create_writer()
      -> enable_if_t<is_class<U>{} || is_enum<U>{}, string> {
    return R"(function (object, size) {
  return write_structures_bounded(object, )" +
           to_js<U>::create_writer() + R"(, size);
})";
  }

  static string create_html() { return R"(this.html_bounded)"; }
};

template <typename T, size_t N> struct to_js<T[N]> {
  static string extent() { return to_string(N); }

  static string create_reader() {
    return R"(function (data, offset) {
  return )" +
           to_js_bounded<T>::create_reader() + R"((data, offset, )" +
           to_string(N) + R"();
})";
  }
  static string create_writer() {
    return R"(function (object) {
  return )" +
           to_js_bounded<T>::create_writer() + R"((object, )" + to_string(N) +
           R"();
})";
  }
  static string create_html() {
    return R"(function (parent, value, dispatcher) {
  return )" +
           to_js_bounded<T>::create_html() + R"((parent, value, dispatcher, )" +
           to_string(N) + R"();
})";
  }
};

template <typename T, size_t M, size_t N> struct to_js<T[M][N]> {
  static string extent() { return to_string(M) + "," + to_js<T[N]>::extent(); }

  template <typename U = typename remove_all_extents<T>::type>
  static auto create_reader() -> enable_if_t<is_arithmetic<U>{}, string> {
    return R"(function (data, offset) {
  return read_multiarray(data, offset, 'get)" +
           js_constructor<U> + R"(', [)" + extent() + R"(]);
})";
  }

  template <typename U = typename remove_all_extents<T>::type>
  static auto create_reader()
      -> enable_if_t<is_class<U>{} || is_enum<U>{}, string> {
    return R"(function (data, offset) {
  return read_multiarray(data, offset, )" +
           to_js<U>::create_reader() + R"(, [)" + extent() + R"(]);
})";
  }

  template <typename U = typename remove_all_extents<T>::type>
  static auto create_writer() -> enable_if_t<is_arithmetic<U>{}, string> {
    return R"(function (object) {
  return write_multiarray(object, 'set)" +
           js_constructor<U> + R"(', [)" + extent() + R"(]);
})";
  }

  template <typename U = typename remove_all_extents<T>::type>
  static auto create_writer()
      -> enable_if_t<is_class<U>{} || is_enum<U>{}, string> {
    return R"(function (object) {
  return write_multiarray(object, )" +
           to_js<U>::create_reader() + R"(, [)" + extent() + R"(]);
})";
  }

  static string create_html() {
    return R"(function (parent, value, dispatcher) {
  return this.html_multiarray(parent, value, dispatcher, html, [)" +
           extent() + R"(]);
})";
  }
};

template <typename T, size_t N> struct to_js<array<T, N>> {
  static string create_reader() {
    return R"(function (data, offset) {
  return )" +
           to_js_bounded<T>::create_reader() + R"((data, offset, )" +
           to_string(N) + R"();
})";
  }
  static string create_writer() {
    return R"(function (object) {
  return )" +
           to_js_bounded<T>::create_writer() + R"((object, )" + to_string(N) +
           R"();
})";
  }
  static string create_html() {
    return R"(function (parent, value, dispatcher) {
  return this.html_bounded(parent, value, dispatcher, )" +
           to_js<T>::create_html() + R"(, )" + to_string(N) + R"();
})";
  }
};

template <typename T, typename U, typename... Traits>
struct to_js<map<T, U, Traits...>> {
  template <typename V = T> static string create_reader() {
    return R"(function (data, offset) {
  return read_associative(data, offset, )" +
           to_js_bounded<T>::create_reader() + R"(, )" +
           to_js_bounded<U>::create_reader() + R"();
})";
  }
  template <typename V = T> static string create_writer() {
    return R"(function (object) {
  return write_associative(object, )" +
           to_js_bounded<T>::create_writer() + R"(, )" +
           to_js_bounded<U>::create_writer() + R"();
})";
  }
  static string create_html() {
    return R"(function (parent, value, dispatcher) {
  return this.html_associative(parent, value, dispatcher, )" +
           to_js<T>::create_html() + R"(, )" + to_js<U>::create_html() + R"();
})";
  }
};

template <typename T, typename U, typename... Traits>
struct to_js<multimap<T, U, Traits...>> : to_js<map<T, U>> {};
template <typename T, typename U, typename... Traits>
struct to_js<unordered_map<T, U, Traits...>> : to_js<map<T, U>> {};
template <typename T, typename U, typename... Traits>
struct to_js<unordered_multimap<T, U, Traits...>> : to_js<map<T, U>> {};

template <typename S, typename T, typename... Ts, typename... Bs>
struct to_js<structure<S, tuple<T, Ts...>, tuple<Bs...>>> {
  static const string names;
  static const string base_names;

  static string create_reader() {
    string base_readers;
    for (auto &readers :
         initializer_list<string>{to_js<Bs>::create_reader()...})
      base_readers += readers + (sizeof...(Bs) ? "," : "");
    if (sizeof...(Bs))
      base_readers.pop_back();
    base_readers = "let base_readers = [" + base_readers + "];\n";

    return R"(function (data, offset) {
  )" + base_names +
           base_readers + names +
           R"(return )" + to_js<tuple<T, Ts...>>::create_reader() +
           R"((data, offset, names, base_readers, base_names);
})";
  }
  static string create_writer() {
    string base_writers;
    for (auto &writers :
         initializer_list<string>{to_js<Bs>::create_writer()...})
      base_writers += writers + (sizeof...(Bs) ? "," : "");
    if (sizeof...(Bs))
      base_writers.pop_back();
    base_writers = "let base_writers = [" + base_writers + "];\n";

    return R"(function (object) {
  )" + base_names +
           base_writers + names +
           R"(return )" + to_js<tuple<T, Ts...>>::create_writer() +
           R"((object, names, base_writers, base_names);
})";
  }
  static string create_html() {
    string base_html;
    for (auto &html : initializer_list<string>{to_js<Bs>::create_html()...})
      base_html += html + (sizeof...(Bs) ? "," : "");
    if (sizeof...(Bs))
      base_html.pop_back();
    base_html = "  let base_html = [" + base_html + "];\n";

    return R"(function (parent, value, dispatcher) {
  )" + base_names +
           base_html + names + '(' + to_js<tuple<T, Ts...>>::create_html() +
           R"().bind(this)(parent, value, dispatcher, names, base_html, base_names);
})";
  }
};

template <typename S, typename T, typename... Ts, typename... Bs>
const string to_js<structure<S, tuple<T, Ts...>, tuple<Bs...>>>::names =
    "let names = [" +
    accumulate(cbegin(structure<S, tuple<T, Ts...>, tuple<Bs...>>::names) + 1,
               cend(structure<S, tuple<T, Ts...>, tuple<Bs...>>::names),
               string{},
               [](const auto &names, const auto &name) {
                 return names + (names.empty() ? "" : ",") + "'" + name + "'";
               }) +
    "];\n";

template <typename S, typename T, typename... Ts, typename... Bs>
const string to_js<structure<S, tuple<T, Ts...>, tuple<Bs...>>>::base_names =
    "let base_names = [" +
    accumulate(cbegin(structure<S, tuple<T, Ts...>, tuple<Bs...>>::base_names),
               cend(structure<S, tuple<T, Ts...>, tuple<Bs...>>::base_names),
               string{},
               [](const auto &names, const auto &name) {
                 return "" + names + (names.empty() ? "" : ",") +
                        (name.empty() ? "" : "'base class " + name + "'");
               }) +
    "];\n";

#define N2W__JS_SPEC(s, m, ...)                                                \
  namespace n2w {                                                              \
  template <>                                                                  \
  struct to_js<s> : to_js<N2W__USING_STRUCTURE(s, m, __VA_ARGS__)> {};         \
  }

#define N2W__JS_CONV(s, c)                                                     \
  namespace n2w {                                                              \
  template <> struct to_js<s> : to_js<c> {};                                   \
  }
}

#endif