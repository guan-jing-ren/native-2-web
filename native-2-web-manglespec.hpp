#ifndef _NATIVE_2_WEB_MANGLESPEC_HPP_
#define _NATIVE_2_WEB_MANGLESPEC_HPP_

#include "native-2-web-common.hpp"
namespace n2w {

using namespace std;
string terminate_processing = "z";

template <typename T> string mangle = terminate_processing;

namespace {
template <typename... Ts> string csv = "";
template <typename T, typename... Ts>
string csv<T, Ts...> = string{mangle<T>} +
                       (sizeof...(Ts) ? "," : "") + csv<Ts...>;
template <typename T, typename U>
string kv = string{mangle<T>} + ':' + string{mangle<U>};
}

template <typename> string mangle_prefix = terminate_processing;
template <typename T, size_t N> string mangle_prefix<T[N]> = "p[";
template <typename T, size_t N> string mangle_prefix<array<T, N>> = "a[";
template <typename T, typename U> string mangle_prefix<pair<T, U>> = "<";
template <typename T, typename... Ts>
string mangle_prefix<tuple<T, Ts...>> = "(";
template <typename T, typename... Traits>
string mangle_prefix<basic_string<T, Traits...>> = "\"";
template <typename T, typename... Traits>
string mangle_prefix<vector<T, Traits...>> = "v[";
template <typename T, typename... Traits>
string mangle_prefix<list<T, Traits...>> = "l[";
template <typename T, typename... Traits>
string mangle_prefix<forward_list<T, Traits...>> = "g[";
template <typename T, typename... Traits>
string mangle_prefix<deque<T, Traits...>> = "d[";
template <typename T, typename... Traits>
string mangle_prefix<set<T, Traits...>> = "s[";
template <typename T, typename U, typename... Traits>
string mangle_prefix<map<T, U, Traits...>> = "m{";
template <typename T, typename... Traits>
string mangle_prefix<unordered_set<T, Traits...>> = "h[";
template <typename T, typename U, typename... Traits>
string mangle_prefix<unordered_map<T, U, Traits...>> = "h{";
template <typename T, typename... Traits>
string mangle_prefix<multiset<T, Traits...>> = "S[";
template <typename T, typename U, typename... Traits>
string mangle_prefix<multimap<T, U, Traits...>> = "M{";
template <typename T, typename... Traits>
string mangle_prefix<unordered_multiset<T, Traits...>> = "H[";
template <typename T, typename U, typename... Traits>
string mangle_prefix<unordered_multimap<T, U, Traits...>> = "H{";
template <typename S, typename T, typename... Ts, typename... Bs>
string mangle_prefix<structure<S, tuple<T, Ts...>, tuple<Bs...>>> = "{";
template <typename R, typename... Ts> string mangle_prefix<R(Ts...)> = "^";

template <> string mangle<void> = "0";
template <> string mangle<bool> = "b";
template <> string mangle<char> = "'3";
template <> string mangle<wchar_t> = "'w";
template <> string mangle<char16_t> = "'4";
template <> string mangle<char32_t> = "'5";
template <> string mangle<int8_t> = "i3";
template <> string mangle<int16_t> = "i4";
template <> string mangle<int32_t> = "i5";
template <> string mangle<int64_t> = "i6";
template <> string mangle<uint8_t> = "u3";
template <> string mangle<uint16_t> = "u4";
template <> string mangle<uint32_t> = "u5";
template <> string mangle<uint64_t> = "u6";
template <> string mangle<float> = "f5";
template <> string mangle<double> = "f6";
template <> string mangle<long double> = "f7";

template <typename T, size_t N>
string mangle<T[N]> = mangle_prefix<T[N]> + string{mangle<T>} + ',' +
                      to_string(N);
template <typename T, size_t N>
string mangle<array<T, N>> = mangle_prefix<array<T, N>> + string{mangle<T>} +
                             ',' + to_string(N);
template <typename T, typename U>
string mangle<pair<T, U>> = mangle_prefix<pair<T, U>> + csv<T, U>;
template <typename T, typename... Ts>
string mangle<tuple<T, Ts...>> =
    mangle_prefix<tuple<T, Ts...>> + csv<T, Ts...> + ')';
template <typename T, typename... Traits>
string mangle<basic_string<T, Traits...>> =
    mangle_prefix<basic_string<T, Traits...>> + string{mangle<T>};
template <typename T, typename... Traits>
string mangle<vector<T, Traits...>> =
    mangle_prefix<vector<T, Traits...>> + string{mangle<T>};
template <typename T, typename... Traits>
string mangle<list<T, Traits...>> =
    mangle_prefix<list<T, Traits...>> + string{mangle<T>};
template <typename T, typename... Traits>
string mangle<forward_list<T, Traits...>> =
    mangle_prefix<forward_list<T, Traits...>> + string{mangle<T>};
template <typename T, typename... Traits>
string mangle<deque<T, Traits...>> =
    mangle_prefix<deque<T, Traits...>> + string{mangle<T>};
template <typename T, typename... Traits>
string mangle<set<T, Traits...>> =
    mangle_prefix<set<T, Traits...>> + string{mangle<T>};
template <typename T, typename U, typename... Traits>
string mangle<map<T, U, Traits...>> =
    mangle_prefix<map<T, U, Traits...>> + kv<T, U>;
template <typename T, typename... Traits>
string mangle<unordered_set<T, Traits...>> =
    mangle_prefix<unordered_set<T, Traits...>> + string{mangle<T>};
template <typename T, typename U, typename... Traits>
string mangle<unordered_map<T, U, Traits...>> =
    mangle_prefix<unordered_map<T, U, Traits...>> + kv<T, U>;
template <typename T, typename... Traits>
string mangle<multiset<T, Traits...>> =
    mangle_prefix<multiset<T, Traits...>> + string{mangle<T>};
template <typename T, typename U, typename... Traits>
string mangle<multimap<T, U, Traits...>> =
    mangle_prefix<multimap<T, U, Traits...>> + kv<T, U>;
template <typename T, typename... Traits>
string mangle<unordered_multiset<T, Traits...>> =
    mangle_prefix<unordered_multiset<T, Traits...>> + string{mangle<T>};

template <typename T, typename U, typename... Traits>
string mangle<unordered_multimap<T, U, Traits...>> =
    mangle_prefix<unordered_multimap<T, U, Traits...>> + kv<T, U>;

template <typename S, typename T, typename... Ts, typename... Bs>
string mangle<structure<S, tuple<T, Ts...>, tuple<Bs...>>> =
    mangle_prefix<structure<S, tuple<T, Ts...>, tuple<Bs...>>> + csv<T, Ts...> +
    '}';

#define N2W__MANGLE_SPEC(s, m, ...)                                            \
  namespace n2w {                                                              \
  template <>                                                                  \
  string mangle<s> = mangle<N2W__USING_STRUCTURE(s, m, __VA_ARGS__)>;          \
  }

template <bool e = __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__>
string endianness = e ? "e" : "E";

template <typename R, typename... Ts>
string mangle<R(Ts...)> = mangle_prefix<R(Ts...)> + string{mangle<R>} + '=' +
                          csv<remove_cv_t<remove_reference_t<Ts>>...>;
template <typename R, typename... Ts>
string mangle<R (*)(Ts...)> = mangle<R(Ts...)>;

template <typename R, typename... Ts>
string function_address(R (*f)(Ts...), uint8_t (&crypt)[sizeof(void (*)())]) {
  uintptr_t obf = 0;
  for (auto i = 0; i < sizeof(f); ++i)
    obf |= static_cast<uintptr_t>(reinterpret_cast<uint8_t *>(&f)[crypt[i]])
           << (i * 8);
  return '@' + to_string(obf) + string{mangle<R(Ts...)>};
}

template <typename R, typename... Ts> string function_address(R (*f)(Ts...)) {
  uint8_t scrambler[] = {0, 1, 2, 3, 4, 5, 6, 7};
  return function_address(f, scrambler);
}

template <typename T> string mangled() { return mangle<T>; }
}

#endif