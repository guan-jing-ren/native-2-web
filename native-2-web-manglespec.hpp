#ifndef _NATIVE_2_WEB_MANGLESPEC_HPP_
#define _NATIVE_2_WEB_MANGLESPEC_HPP_

#include "native-2-web-common.hpp"
#include <iostream>
namespace n2w {

using namespace std;
constexpr auto terminate_processing = "z";

template <typename T> constexpr auto mangle = terminate_processing;

namespace {
template <typename... Ts> const auto csv = "";
template <typename T, typename... Ts>
const auto csv<T, Ts...> = csv<T> + ',' + csv<Ts...>;
template <typename T> const string csv<T> = mangle<T>;
template <typename T, typename U>
const auto kv = string{mangle<T>} + ':' + mangle<U>;
}

template <typename> constexpr auto mangle_prefix = terminate_processing;
template <typename T, size_t N> constexpr auto mangle_prefix<T[N]> = "p[";
template <typename T, size_t N>
constexpr auto mangle_prefix<array<T, N>> = "a[";
template <typename T, typename U>
constexpr auto mangle_prefix<pair<T, U>> = '<';
template <typename T, typename... Ts>
constexpr auto mangle_prefix<tuple<T, Ts...>> = '(';
template <typename T, typename... Traits>
constexpr auto mangle_prefix<basic_string<T, Traits...>> = '"';
template <typename T, typename... Traits>
constexpr auto mangle_prefix<vector<T, Traits...>> = "v[";
template <typename T, typename... Traits>
constexpr auto mangle_prefix<list<T, Traits...>> = "l[";
template <typename T, typename... Traits>
constexpr auto mangle_prefix<forward_list<T, Traits...>> = "g[";
template <typename T, typename... Traits>
constexpr auto mangle_prefix<deque<T, Traits...>> = "d[";
template <typename T, typename... Traits>
constexpr auto mangle_prefix<set<T, Traits...>> = "s[";
template <typename T, typename U, typename... Traits>
constexpr auto mangle_prefix<map<T, U, Traits...>> = "m{";
template <typename T, typename... Traits>
constexpr auto mangle_prefix<unordered_set<T, Traits...>> = "h[";
template <typename T, typename U, typename... Traits>
constexpr auto mangle_prefix<unordered_map<T, U, Traits...>> = "h{";
template <typename T, typename... Traits>
constexpr auto mangle_prefix<multiset<T, Traits...>> = "S[";
template <typename T, typename U, typename... Traits>
constexpr auto mangle_prefix<multimap<T, U, Traits...>> = "M{";
template <typename T, typename... Traits>
constexpr auto mangle_prefix<unordered_multiset<T, Traits...>> = "H[";
template <typename T, typename U, typename... Traits>
constexpr auto mangle_prefix<unordered_multimap<T, U, Traits...>> = "H{";
template <typename S, typename T, typename... Ts>
const auto mangle_prefix<structure<S, T, Ts...>> = '{';
template <typename R, typename... Ts>
constexpr auto mangle_prefix<R(Ts...)> = '^';

template <> constexpr auto mangle<void> = '0';
template <> constexpr auto mangle<bool> = 'b';
template <> constexpr auto mangle<wchar_t> = "'w";
template <> constexpr auto mangle<char16_t> = "'4";
template <> constexpr auto mangle<char32_t> = "'5";
template <> constexpr auto mangle<int8_t> = "i3";
template <> constexpr auto mangle<int16_t> = "i4";
template <> constexpr auto mangle<int32_t> = "i5";
template <> constexpr auto mangle<int64_t> = "i6";
template <> constexpr auto mangle<uint8_t> = "u3";
template <> constexpr auto mangle<uint16_t> = "u4";
template <> constexpr auto mangle<uint32_t> = "u5";
template <> constexpr auto mangle<uint64_t> = "u6";
template <> constexpr auto mangle<float> = "f5";
template <> constexpr auto mangle<double> = "f6";
template <> constexpr auto mangle<long double> = "f7";

template <typename T, size_t N>
const auto mangle<T[N]> = mangle_prefix<T[N]> + string{mangle<T>} + ',' +
                          to_string(N);
template <typename T, size_t N>
const auto mangle<array<T, N>> = mangle_prefix<array<T, N>> +
                                 string{mangle<T>} + ',' + to_string(N);
template <typename T, typename U>
const auto mangle<pair<T, U>> = mangle_prefix<pair<T, U>> + csv<T, U>;
template <typename T, typename... Ts>
const auto mangle<tuple<T, Ts...>> =
    mangle_prefix<tuple<T, Ts...>> + csv<T, Ts...> + ')';
template <typename T, typename... Traits>
const auto mangle<basic_string<T, Traits...>> =
    mangle_prefix<basic_string<T, Traits...>> + string{mangle<T>};
template <typename T, typename... Traits>
const auto mangle<vector<T, Traits...>> =
    mangle_prefix<vector<T, Traits...>> + string{mangle<T>};
template <typename T, typename... Traits>
const auto mangle<list<T, Traits...>> =
    mangle_prefix<list<T, Traits...>> + string{mangle<T>};
template <typename T, typename... Traits>
const auto mangle<forward_list<T, Traits...>> =
    mangle_prefix<forward_list<T, Traits...>> + string{mangle<T>};
template <typename T, typename... Traits>
const auto mangle<deque<T, Traits...>> =
    mangle_prefix<deque<T, Traits...>> + string{mangle<T>};
template <typename T, typename... Traits>
const auto mangle<set<T, Traits...>> =
    mangle_prefix<set<T, Traits...>> + string{mangle<T>};
template <typename T, typename U, typename... Traits>
const auto mangle<map<T, U, Traits...>> =
    mangle_prefix<map<T, U, Traits...>> + kv<T, U>;
template <typename T, typename... Traits>
const auto mangle<unordered_set<T, Traits...>> =
    mangle_prefix<unordered_set<T, Traits...>> + string{mangle<T>};
template <typename T, typename U, typename... Traits>
const auto mangle<unordered_map<T, U, Traits...>> =
    mangle_prefix<unordered_map<T, U, Traits...>> + kv<T, U>;
template <typename T, typename... Traits>
const auto mangle<multiset<T, Traits...>> =
    mangle_prefix<multiset<T, Traits...>> + string{mangle<T>};
template <typename T, typename U, typename... Traits>
const auto mangle<multimap<T, U, Traits...>> =
    mangle_prefix<multimap<T, U, Traits...>> + kv<T, U>;
template <typename T, typename... Traits>
const auto mangle<unordered_multiset<T, Traits...>> =
    mangle_prefix<unordered_multiset<T, Traits...>> + string{mangle<T>};

template <typename T, typename U, typename... Traits>
const auto mangle<unordered_multimap<T, U, Traits...>> =
    mangle_prefix<unordered_multimap<T, U, Traits...>> + kv<T, U>;

template <typename S, typename T, typename... Ts>
const auto mangle<structure<S, T, Ts...>> =
    mangle_prefix<structure<S, T, Ts...>> + csv<T, Ts...> + '}';

template <bool e = __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__>
constexpr auto endianness = e ? "e" : "E";

template <typename R, typename... Ts>
const auto mangle<R(Ts...)> = mangle_prefix<R(Ts...)> + string{mangle<R>} +
                              '=' + csv<remove_cv_t<remove_reference_t<Ts>>...>;
template <typename R, typename... Ts>
const auto mangle<R (*)(Ts...)> = mangle<R(Ts...)>;

template <typename R, typename... Ts>
const string function_address(R (*f)(Ts...),
                              const uint8_t (&crypt)[sizeof(void (*)())]) {
  uintptr_t obf = 0;
  for (auto i = 0; i < sizeof(f); ++i)
    obf |= static_cast<uintptr_t>(reinterpret_cast<uint8_t *>(&f)[crypt[i]])
           << (i * 8);
  return '@' + to_string(obf) + mangle<R(Ts...)>;
}

template <typename R, typename... Ts>
const string function_address(R (*f)(Ts...)) {
  uint8_t scrambler[] = {0, 1, 2, 3, 4, 5, 6, 7};
  return function_address(f, scrambler);
}
}

#endif