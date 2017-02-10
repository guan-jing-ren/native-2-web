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
  return read_number(data, offset, )" +
        js_constructor<U> + R"(Array);
})";
  }
};

template <typename T, typename... Ts> struct to_js_homogenous {
  template <size_t... Is> static string create_reader() {
    return to_js<T>::create_reader() + ",\n" +
           to_js_homogenous<Ts...>::create_reader();
  }
};

template <typename T> struct to_js_homogenous<T> {
  template <size_t... Is> static string create_reader() {
    return to_js<T>::create_reader();
  }
};

template <typename T, typename U> struct to_js<pair<T, U>> {
  static string create_reader() {
    return R"(function (data, offset) {
  return read_structure(data, offset, [)" +
           to_js_homogenous<T, U>::create_reader() +
           R"(]);
})";
  }
};

template <typename T, typename... Ts> struct to_js<tuple<T, Ts...>> {
  static string create_reader() {
    return R"(function (data, offset) {
  return read_structure(data, offset, [)" +
           to_js_homogenous<T, Ts...>::create_reader() +
           R"(]);
})";
  }
};

template <typename T, typename... Traits>
struct to_js<basic_string<T, Traits...>> {
  static string create_reader() { return "read_string"; }
};

template <typename T, typename... Traits> struct to_js<vector<T, Traits...>> {
  template <typename U = T>
  static auto create_reader() -> enable_if_t<is_arithmetic<U>{}, string> {
    return R"(function (data, offset) {
  return read_numbers(data, offset, )" +
           js_constructor<U> + R"(Array);
})";
  }

  template <typename U = T>
  static auto create_reader() -> enable_if_t<is_class<U>{}, string> {
    return R"(function (data, offset) {
  return read_structures(data, offset, )" +
           to_js<U>::create_reader() + R"();
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
  return read_numbers_bounded(data, offset, )" +
           js_constructor<U> + R"(Array, size);
})";
  }
  template <typename U = T>
  static auto create_reader() -> enable_if_t<is_class<U>{}, string> {
    return R"(function (data, offset, size) {
  return read_structures_bounded(data, offset, )" +
           to_js<U>::create_reader() + R"(, size);
})";
  }
};

template <typename T, size_t N> struct to_js<T[N]> {
  static string extent() { return to_string(N); }

  static string create_reader() {
    return R"(function (data, offset) {
  return )" +
           to_js_bounded<T>::create_reader() + R"((data, offset, )" +
           to_string(N) +
           R"();
})";
  }
};

template <typename T, size_t M, size_t N> struct to_js<T[M][N]> {
  static string extent() { return to_string(M) + "," + to_js<T[N]>::extent(); }

  template <typename U = typename remove_all_extents<T>::type>
  static auto create_reader() -> enable_if_t<is_arithmetic<U>{}, string> {
    return R"(function (data, offset) {
  return read_multiarray(data, offset, )" +
           js_constructor<U> + R"(Array, [)" + extent() + R"(]);
})";
  }

  template <typename U = typename remove_all_extents<T>::type>
  static auto create_reader() -> enable_if_t<is_class<U>{}, string> {
    return R"(function (data, offset) {
  return read_multiarray(data, offset, )" +
           to_js<U>::create_reader() + R"(, [)" + extent() + R"(]);
})";
  }
};

template <typename T, size_t N> struct to_js<array<T, N>> {
  static string create_reader() {
    return R"(function (data, offset) {
  return )" +
           to_js_bounded<T>::create_reader() + R"((data, offset, )" +
           to_string(N) +
           R"();
})";
  }
};

template <typename T, typename U, typename... Traits>
struct to_js<map<T, U, Traits...>> {
  template <typename V = T> static string create_reader() {
    return R"(function (data, offset) {
  return read_associative(data, offset, )" +
           to_js_bounded<T>::create_reader() + R"(, )" +
           to_js_bounded<U>::create_reader() +
           R"(
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
  static string create_reader() {
    auto names = accumulate(begin(structure<S, T, Ts...>::names) + 1,
                            end(structure<S, T, Ts...>::names), string{},
                            [](const auto &names, const auto &name) {
                              return names + (names.empty() ? "" : ",") + name;
                            });

    return R"(function (data, offset) {
  let tuple = )" +
           to_js<tuple<T, Ts...>>::create_reader() + R"((data, offset);
  let names = [)" +
           names +
           R"(];
  return names.reduce((p,c,i) => {p[c] = tuple[i]; return p;}, {});
})";
  }
};

#define JS_SPEC(s, m)                                                          \
  namespace n2w {                                                              \
  template <> struct to_js<s> : to_js<USING_STRUCTURE(s, m)> {};               \
  }
}

#endif