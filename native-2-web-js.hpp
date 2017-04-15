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
    return
        R"(function (data, offset) {
  return read_number(data, offset, 'get)" +
        js_constructor<U> + R"(');
})";
  }
  template <typename U = T>
  static auto create_writer() -> enable_if_t<is_arithmetic<U>{}, string> {
    return
        R"(function (object) {
  return write_number(object, 'set)" +
        js_constructor<U> + R"(');
})";
  }
  template <typename U = T>
  static auto create_html() -> enable_if_t<is_arithmetic<U>{}, string> {
    return R"(html_number)";
  }
};

template <> struct to_js<char> {
  static string create_reader() { return R"(read_char)"; }
  static string create_writer() { return R"(write_char)"; }
  static string create_html() { return R"(html_char)"; }
};
template <> struct to_js<char32_t> {
  static string create_reader() { return R"(read_char32)"; }
  static string create_writer() { return R"(write_char32)"; }
  static string create_html() { return R"(html_char32)"; }
};

template <> struct to_js<char16_t> : to_js<char32_t> {};
template <> struct to_js<wchar_t> : to_js<char32_t> {};

template <typename T, typename... Ts> struct to_js_heterogenous {
  template static string create_reader() {
    return to_js<T>::create_reader() + ",\n" +
           to_js_heterogenous<Ts...>::create_reader();
  }
  template static string create_writer() {
    return to_js<T>::create_writer() + ",\n" +
           to_js_heterogenous<Ts...>::create_writer();
  }
  template static string create_html() {
    return to_js<T>::create_html() + ",\n" +
           to_js_heterogenous<Ts...>::create_html();
  }
};

template <typename T> struct to_js_heterogenous<T> {
  template static string create_reader() { return to_js<T>::create_reader(); }
  template static string create_writer() { return to_js<T>::create_writer(); }
  template static string create_html() { return to_js<T>::create_html(); }
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
  return html_structure(parent, value, dispatcher, [)" +
           to_js_heterogenous<T, U>::create_html() + R"(], ['first', 'second']);
})";
  }
};

template <typename T, typename... Ts> struct to_js<tuple<T, Ts...>> {
  static string create_reader() {
    return R"(function (data, offset, names) {
  return read_structure(data, offset, [)" +
           to_js_heterogenous<T, Ts...>::create_reader() + R"(], names);
})";
  }
  static string create_writer() {
    return R"(function (object, names) {
  return write_structure(object, [)" +
           to_js_heterogenous<T, Ts...>::create_writer() + R"(], names);
})";
  }
  static string create_html() {
    return R"(function(parent, value, dispatcher, names) {
  return html_structure(parent, value, dispatcher, [)" +
           to_js_heterogenous<T, Ts...>::create_html() + R"(], names);
})";
  }
};

template <typename T, typename... Traits>
struct to_js<basic_string<T, Traits...>> {
  static string create_reader() { return "read_string"; }
  static string create_writer() { return "write_string"; }
  static string create_html() { return "html_string"; }
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
  static auto create_reader() -> enable_if_t<is_class<U>{}, string> {
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
  static auto create_writer() -> enable_if_t<is_class<U>{}, string> {
    return R"(function (object) {
  return write_structures(object, )" +
           to_js<U>::create_writer() + R"();
})";
  }

  static auto create_html() {
    return R"(function(parent, value, dispatcher) {
  return html_sequence(parent, value, dispatcher, )" +
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
  static auto create_reader() -> enable_if_t<is_class<U>{}, string> {
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
  static auto create_writer() -> enable_if_t<is_class<U>{}, string> {
    return R"(function (object, size) {
  return write_structures_bounded(object, )" +
           to_js<U>::create_writer() + R"(, size);
})";
  }

  static string create_html() { return R"(html_bounded)"; }
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
  static auto create_reader() -> enable_if_t<is_class<U>{}, string> {
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
  static auto create_writer() -> enable_if_t<is_class<U>{}, string> {
    return R"(function (object) {
  return write_multiarray(object, )" +
           to_js<U>::create_reader() + R"(, [)" + extent() + R"(]);
})";
  }

  static string create_html() {
    return R"(function (parent, value, dispatcher) {
  return html_multiarray(parent, value, dispatcher, html, [)" +
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
  return html_bounded(parent, value, dispatcher, )" +
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
    return R"(function(parent, value, dispatcher) {
  return html_associative(parent, value, dispatcher, )" +
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

template <typename S, typename T, typename... Ts>
struct to_js<structure<S, T, Ts...>> {
  static const string names;

  static string create_reader() {
    return R"(function (data, offset) {
  )" + names +
           R"(
  return )" +
           to_js<tuple<T, Ts...>>::create_reader() + R"((data, offset, names);
})";
  }
  static string create_writer() {
    return R"(function (object) {
  )" + names +
           R"(
  return )" +
           to_js<tuple<T, Ts...>>::create_writer() + R"((object, names);
})";
  }
  static string create_html() {
    return R"(function (parent, value, dispatcher) {
  )" + names +
           R"(
  return )" +
           to_js<tuple<T, Ts...>>::create_html() +
           R"((parent, value, dispatcher, names);
})";
  }
};

template <typename S, typename T, typename... Ts>
const string to_js<structure<S, T, Ts...>>::names =
    "let names = [" +
    accumulate(begin(structure<S, T, Ts...>::names) + 1,
               end(structure<S, T, Ts...>::names), string{},
               [](const auto &names, const auto &name) {
                 return names + (names.empty() ? "" : ",") + "'" + name + "'";
               }) +
    "];";

#define JS_SPEC(s, m)                                                          \
  namespace n2w {                                                              \
  template <> struct to_js<s> : to_js<USING_STRUCTURE(s, m)> {};               \
  }
}

#endif