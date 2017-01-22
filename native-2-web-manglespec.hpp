#ifndef _NATIVE_2_WEB_MANGLESPEC_HPP_
#define _NATIVE_2_WEB_MANGLESPEC_HPP_

#include "native-2-web-common.hpp"
namespace n2w {

using namespace std;
constexpr auto terminate_processing = "z";

template <typename T> const string mangle = terminate_processing;

namespace {
template <typename... Ts> const string csv = "";
template <typename T, typename... Ts>
const string csv<T, Ts...> = csv<T> + "," + csv<Ts...>;
template <typename T> const string csv<T> = mangle<T>;
template <typename T, typename U> const string kv = mangle<T> + ":" + mangle<U>;
}

template <typename> const string mangle_prefix = terminate_processing;
template <typename T, size_t N> const string mangle_prefix<T[N]> = "p[";
template <typename T, size_t N> const string mangle_prefix<array<T, N>> = "a[";
template <typename T, typename U> const string mangle_prefix<pair<T, U>> = "<";
template <typename T, typename... Ts>
const string mangle_prefix<tuple<T, Ts...>> = "(";
template <typename T, typename... Traits>
const string mangle_prefix<basic_string<T, Traits...>> = "\"";
template <typename T, typename... Traits>
const string mangle_prefix<vector<T, Traits...>> = "v[";
template <typename T, typename... Traits>
const string mangle_prefix<list<T, Traits...>> = "l[";
template <typename T, typename... Traits>
const string mangle_prefix<forward_list<T, Traits...>> = "g[";
template <typename T, typename... Traits>
const string mangle_prefix<deque<T, Traits...>> = "d[";
template <typename T, typename... Traits>
const string mangle_prefix<set<T, Traits...>> = "s[";
template <typename T, typename U, typename... Traits>
const string mangle_prefix<map<T, U, Traits...>> = "m{";
template <typename T, typename... Traits>
const string mangle_prefix<unordered_set<T, Traits...>> = "h[";
template <typename T, typename U, typename... Traits>
const string mangle_prefix<unordered_map<T, U, Traits...>> = "h{";
template <typename T, typename... Traits>
const string mangle_prefix<multiset<T, Traits...>> = "S[";
template <typename T, typename U, typename... Traits>
const string mangle_prefix<multimap<T, U, Traits...>> = "M{";
template <typename T, typename... Traits>
const string mangle_prefix<unordered_multiset<T, Traits...>> = "H[";
template <typename T, typename U, typename... Traits>
const string mangle_prefix<unordered_multimap<T, U, Traits...>> = "H{";
template <typename S, typename T, typename... Ts>
const string mangle_prefix<structure<S, T, Ts...>> = "{";
template <typename R, typename... Ts>
const string mangle_prefix<R(Ts...)> = "^";

template <> constexpr auto mangle<void> = "0";
template <> constexpr auto mangle<bool> = "b";
template <> constexpr auto mangle<char> = "'3";
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
const string mangle<T[N]> = mangle_prefix<T[N]> + mangle<T> + "," +
                            to_string(N);
template <typename T, size_t N>
const string mangle<array<T, N>> = mangle_prefix<array<T, N>> + mangle<T> +
                                   "," + to_string(N);
template <typename T, typename U>
const string mangle<pair<T, U>> = mangle_prefix<pair<T, U>> + csv<T, U>;
template <typename T, typename... Ts>
const string mangle<tuple<T, Ts...>> =
    mangle_prefix<tuple<T, Ts...>> + csv<T, Ts...> + ")";
template <typename T, typename... Traits>
const string mangle<basic_string<T, Traits...>> =
    mangle_prefix<basic_string<T, Traits...>> + mangle<T>;
template <typename T, typename... Traits>
const string mangle<vector<T, Traits...>> =
    mangle_prefix<vector<T, Traits...>> + mangle<T>;
template <typename T, typename... Traits>
const string mangle<list<T, Traits...>> =
    mangle_prefix<list<T, Traits...>> + mangle<T>;
template <typename T, typename... Traits>
const string mangle<forward_list<T, Traits...>> =
    mangle_prefix<forward_list<T, Traits...>> + mangle<T>;
template <typename T, typename... Traits>
const string mangle<deque<T, Traits...>> =
    mangle_prefix<deque<T, Traits...>> + mangle<T>;
template <typename T, typename... Traits>
const string mangle<set<T, Traits...>> =
    mangle_prefix<set<T, Traits...>> + mangle<T>;
template <typename T, typename U, typename... Traits>
const string mangle<map<T, U, Traits...>> =
    mangle_prefix<map<T, U, Traits...>> + kv<T, U>;
template <typename T, typename... Traits>
const string mangle<unordered_set<T, Traits...>> =
    mangle_prefix<unordered_set<T, Traits...>> + mangle<T>;
template <typename T, typename U, typename... Traits>
const string mangle<unordered_map<T, U, Traits...>> =
    mangle_prefix<unordered_map<T, U, Traits...>> + kv<T, U>;
template <typename T, typename... Traits>
const string mangle<multiset<T, Traits...>> =
    mangle_prefix<multiset<T, Traits...>> + mangle<T>;
template <typename T, typename U, typename... Traits>
const string mangle<multimap<T, U, Traits...>> =
    mangle_prefix<multimap<T, U, Traits...>> + kv<T, U>;
template <typename T, typename... Traits>
const string mangle<unordered_multiset<T, Traits...>> =
    mangle_prefix<unordered_multiset<T, Traits...>> + mangle<T>;

template <typename T, typename U, typename... Traits>
const string mangle<unordered_multimap<T, U, Traits...>> =
    mangle_prefix<unordered_multimap<T, U, Traits...>> + kv<T, U>;

template <typename S, typename T, typename... Ts>
const string mangle<structure<S, T, Ts...>> =
    mangle_prefix<structure<S, T, Ts...>> + csv<T, Ts...> + "}";

#define MANGLE_SPEC(s, m)                                                      \
  namespace n2w {                                                              \
  template <> const string mangle<s> = mangle<USING_STRUCTURE(s, m)>;          \
  }

template <bool e = __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__>
const string endianness = e ? "e" : "E";

template <typename R, typename... Ts>
const string mangle<R(Ts...)> = mangle_prefix<R(Ts...)> + mangle<R> + "=" +
                                csv<remove_cv_t<remove_reference_t<Ts>>...>;
template <typename R, typename... Ts>
const string mangle<R (*)(Ts...)> = mangle<R(Ts...)>;

template <typename R, typename... Ts>
const string function_address(R (*f)(Ts...),
                              const uint8_t (&crypt)[sizeof(void (*)())]) {
  uintptr_t obf = 0;
  for (auto i = 0; i < sizeof(f); ++i)
    obf |= static_cast<uintptr_t>(reinterpret_cast<uint8_t *>(&f)[crypt[i]])
           << (i * 8);
  return "@" + to_string(obf) + mangle<R(Ts...)>;
}

template <typename R, typename... Ts>
const string function_address(R (*f)(Ts...)) {
  constexpr uint8_t scrambler[] = {0, 1, 2, 3, 4, 5, 6, 7};
  return function_address(f, scrambler);
}
}

#endif