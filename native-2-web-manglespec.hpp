#ifndef _NATIVE_2_WEB_MANGLESPEC_HPP_
#define _NATIVE_2_WEB_MANGLESPEC_HPP_

#include "native-2-web-common.hpp"

namespace n2w {

using namespace std;
template <typename T> constexpr auto mangle = "";

namespace {
template <typename T, typename... Ts>
const auto csv = csv<T> + ',' + csv<Ts...>;
template <typename T> const string csv<T> = mangle<T>;
template <typename T, typename U>
const auto kv = string{mangle<T>} + ':' + mangle<U>;
}

template <typename> constexpr auto mangle_prefix = "";
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

template <typename... Ts> struct structure {};

template <typename... Ts> const auto mangle_prefix<structure<Ts...>> = '{';
template <typename... Ts>
const auto mangle<structure<Ts...>> =
    mangle_prefix<structure<Ts...>> + csv<Ts...> + '}';

template <bool e = __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__>
constexpr auto endianness = e ? "e" : "E";
}

#endif