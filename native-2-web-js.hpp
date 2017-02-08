#ifndef _NATIVE_2_WEB_JS_HPP_
#define _NATIVE_2_WEB_JS_HPP_

#include "native-2-web-common.hpp"

namespace n2w {
using namespace std;

template <typename T> string constructor = "";
template <> const string constructor<bool> = "Uint8";
template <> const string constructor<char> = "Int8";
template <> const string constructor<wchar_t> = "Uint32";
template <> const string constructor<char16_t> = "Uint16";
template <> const string constructor<char32_t> = "Uint32";
template <> const string constructor<int8_t> = "Int8";
template <> const string constructor<int16_t> = "Int16";
template <> const string constructor<int32_t> = "Int32";
template <> const string constructor<uint8_t> = "Uint8";
template <> const string constructor<uint16_t> = "Uint16";
template <> const string constructor<uint32_t> = "Uint32";
template <> const string constructor<float> = "Float32";
template <> const string constructor<double> = "Float64";

template <typename T> struct to_js {
  template <typename U = T>
  static auto create() -> enable_if_t<is_arithmetic<U>{}, string> {
    return
        R"(function (data, offset) {
  return read_number(data, offset, )" +
        constructor<U> + R"(Array);
})";
  }
};

template <typename... Traits> struct to_js<basic_string<char, Traits...>> {
  static string create() { return "read_string"; }
};

template <typename T, typename... Traits> struct to_js<vector<T, Traits...>> {
  template <typename U = T>
  static auto create() -> enable_if_t<is_arithmetic<U>{}, string> {
    return R"(function (data, offset) {
  return read_numbers(data, offset, )" +
           constructor<U> + R"(Array);
})";
  }

  template <typename U = T>
  static auto create() -> enable_if_t<is_class<U>{}, string> {
    return R"(function (data, offset) {
  return read_structures(data, offset, )" +
           to_js<U>::create() + R"();
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
  static auto create() -> enable_if_t<is_arithmetic<U>{}, string> {
    return R"(function (data, offset, size) {
  return read_numbers_bounded(data, offset, )" +
           constructor<U> + R"(Array, size);
})";
  }
  template <typename U = T>
  static auto create() -> enable_if_t<is_class<U>{}, string> {
    return R"(function (data, offset, size) {
  return read_structures_bounded(data, offset, )" +
           to_js<U>::create() + R"(, size);
})";
  }
};

template <typename T, size_t N> struct to_js<T[N]> {
  static string extent() { return to_string(N); }

  static string create() {
    return R"(function (data, offset) {
  return )" +
           to_js_bounded<T>::create() + R"((data, offset, )" + to_string(N) +
           R"();
})";
  }
};

template <typename T, size_t M, size_t N> struct to_js<T[M][N]> {
  static string extent() { return to_string(M) + "," + to_js<T[N]>::extent(); }

  template <typename U = typename remove_all_extents<T>::type>
  static auto create() -> enable_if_t<is_arithmetic<U>{}, string> {
    return R"(function (data, offset) {
  return read_multiarray(data, offset, )" +
           constructor<U> + R"(Array, [)" + extent() + R"(]);
})";
  }

  template <typename U = typename remove_all_extents<T>::type>
  static auto create() -> enable_if_t<is_class<U>{}, string> {
    return R"(function (data, offset) {
  return read_multiarray(data, offset, )" +
           to_js<U>::create() + R"(, [)" + extent() + R"(]);
})";
  }
};

template <typename T, typename U, typename... Traits>
struct to_js<map<T, U, Traits...>> {
  template <typename V = T> static string create() {
    return R"(function (data, offset) {
  return read_associative(data, offset, )" +
           to_js_bounded<T>::create() + R"(, )" + to_js_bounded<U>::create() +
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
}

#endif