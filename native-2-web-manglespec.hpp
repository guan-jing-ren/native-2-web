#ifndef _NATIVE_2_WEB_MANGLESPEC_HPP_
#define _NATIVE_2_WEB_MANGLESPEC_HPP_

#include "native-2-web-common.hpp"
namespace n2w {

using namespace std;
string terminate_processing = "z";

template <typename...> struct mangle {
  static string value() { return terminate_processing; }
};
template <typename...> struct mangle_prefix {
  static string value() { return terminate_processing; }
};

template <typename T> string mangled() {
  return mangle<remove_cv_t<remove_reference_t<T>>>::value();
}
template <typename T> string mangled(const T &&) { return mangled<T>(); }

template <typename T> string mangle_prefixed() {
  return mangle_prefix<remove_cv_t<remove_reference_t<T>>>::value();
}
template <typename T> string mangle_prefixed(const T &&) {
  return mangle_prefixed<T>();
}

namespace {
template <typename... Ts> struct csv {
  static string value() { return ""; }
};
template <typename T, typename... Ts> struct csv<T, Ts...> {
  static string value() {
    return mangled<T>() + (sizeof...(Ts) ? "," : "") + csv<Ts...>::value();
  }
};
template <typename T, typename U> struct kv {
  static string value() { return mangled<T>() + "&" + mangled<U>(); }
};
}

template <typename E>
struct mangle_prefix<E> : enable_if_t<is_enum<E>::value, true_type> {
  static string value() { return "c["; }
};
template <typename T, size_t N> struct mangle_prefix<T[N]> {
  static string value() { return "p["; }
};
template <typename T, size_t N> struct mangle_prefix<array<T, N>> {
  static string value() { return "a["; }
};
template <typename T, typename U> struct mangle_prefix<pair<T, U>> {
  static string value() { return "<"; }
};
template <typename T, typename... Ts> struct mangle_prefix<tuple<T, Ts...>> {
  static string value() { return "("; }
};
template <typename T, typename... Traits>
struct mangle_prefix<basic_string<T, Traits...>> {
  static string value() { return "\""; }
};
template <typename T, typename... Traits>
struct mangle_prefix<vector<T, Traits...>> {
  static string value() { return "v["; }
};
template <typename T, typename... Traits>
struct mangle_prefix<list<T, Traits...>> {
  static string value() { return "l["; }
};
template <typename T, typename... Traits>
struct mangle_prefix<forward_list<T, Traits...>> {
  static string value() { return "g["; }
};
template <typename T, typename... Traits>
struct mangle_prefix<deque<T, Traits...>> {
  static string value() { return "d["; }
};
template <typename T, typename... Traits>
struct mangle_prefix<set<T, Traits...>> {
  static string value() { return "s["; }
};
template <typename T, typename U, typename... Traits>
struct mangle_prefix<map<T, U, Traits...>> {
  static string value() { return "m{"; }
};
template <typename T, typename... Traits>
struct mangle_prefix<unordered_set<T, Traits...>> {
  static string value() { return "h["; }
};
template <typename T, typename U, typename... Traits>
struct mangle_prefix<unordered_map<T, U, Traits...>> {
  static string value() { return "h{"; }
};
template <typename T, typename... Traits>
struct mangle_prefix<multiset<T, Traits...>> {
  static string value() { return "S["; }
};
template <typename T, typename U, typename... Traits>
struct mangle_prefix<multimap<T, U, Traits...>> {
  static string value() { return "M{"; }
};
template <typename T, typename... Traits>
struct mangle_prefix<unordered_multiset<T, Traits...>> {
  static string value() { return "H["; }
};
template <typename T, typename U, typename... Traits>
struct mangle_prefix<unordered_multimap<T, U, Traits...>> {
  static string value() { return "H{"; }
};
template <typename S, typename T, typename... Ts, typename... Bs>
struct mangle_prefix<structure<S, tuple<T, Ts...>, tuple<Bs...>>> {
  static string value() { return "{"; }
};
template <typename T> struct mangle_prefix<optional<T>> {
  static string value() { return "?"; }
};
template <typename... Ts> struct mangle_prefix<variant<Ts...>> {
  static string value() { return "|["; }
};
template <intmax_t N, intmax_t D> struct mangle_prefix<ratio<N, D>> {
  static string value() { return "%"; }
};
template <typename R, intmax_t N, intmax_t D>
struct mangle_prefix<chrono::duration<R, ratio<N, D>>> {
  static string value() { return ":"; }
};
template <size_t N> struct mangle_prefix<bitset<N>> {
  static string value() { return "!"; }
};
template <typename R, typename... Ts> struct mangle_prefix<R(Ts...)> {
  static string value() { return "^"; }
};

template <> struct mangle<void> {
  static string value() { return "0"; }
};
template <> struct mangle<void *> {
  static string value() { return "0"; }
};
template <> struct mangle<bool> {
  static string value() { return "b"; }
};
template <> struct mangle<char> {
  static string value() { return "'3"; }
};
template <> struct mangle<wchar_t> {
  static string value() { return "'w"; }
};
template <> struct mangle<char16_t> {
  static string value() { return "'4"; }
};
template <> struct mangle<char32_t> {
  static string value() { return "'5"; }
};
template <> struct mangle<int8_t> {
  static string value() { return "i3"; }
};
template <> struct mangle<int16_t> {
  static string value() { return "i4"; }
};
template <> struct mangle<int32_t> {
  static string value() { return "i5"; }
};
template <> struct mangle<int64_t> {
  static string value() { return "i6"; }
};
template <> struct mangle<uint8_t> {
  static string value() { return "u3"; }
};
template <> struct mangle<uint16_t> {
  static string value() { return "u4"; }
};
template <> struct mangle<uint32_t> {
  static string value() { return "u5"; }
};
template <> struct mangle<uint64_t> {
  static string value() { return "u6"; }
};
template <> struct mangle<float> {
  static string value() { return "f5"; }
};
template <> struct mangle<double> {
  static string value() { return "f6"; }
};
template <> struct mangle<long double> {
  static string value() { return "f7"; }
};
template <> struct mangle<complex<float>> {
  static string value() { return "j5"; }
};
template <> struct mangle<complex<double>> {
  static string value() { return "j6"; }
};
template <> struct mangle<complex<long double>> {
  static string value() { return "j7"; }
};

template <typename T, size_t N> struct mangle<T[N]> {
  static string value() {
    return mangle_prefixed<T[N]>() + mangled<T>() + "," + to_string(N);
  }
};
template <typename T, size_t N> struct mangle<array<T, N>> {
  static string value() {
    return mangle_prefixed<array<T, N>>() + mangled<T>() + "," + to_string(N);
  }
};
template <typename T, typename U> struct mangle<pair<T, U>> {
  static string value() {
    return mangle_prefixed<pair<T, U>>() + csv<T, U>::value();
  }
};
template <typename T, typename... Ts> struct mangle<tuple<T, Ts...>> {
  static string value() {
    return mangle_prefixed<tuple<T, Ts...>>() + csv<T, Ts...>::value() + ")";
  }
};
template <typename T, typename... Traits>
struct mangle<basic_string<T, Traits...>> {
  static string value() {
    return mangle_prefixed<basic_string<T, Traits...>>() + mangled<T>();
  }
};
template <typename T, typename... Traits> struct mangle<vector<T, Traits...>> {
  static string value() {
    return mangle_prefixed<vector<T, Traits...>>() + mangled<T>();
  }
};
template <typename T, typename... Traits> struct mangle<list<T, Traits...>> {
  static string value() {
    return mangle_prefixed<list<T, Traits...>>() + mangled<T>();
  }
};
template <typename T, typename... Traits>
struct mangle<forward_list<T, Traits...>> {
  static string value() {
    return mangle_prefixed<forward_list<T, Traits...>>() + mangled<T>();
  }
};
template <typename T, typename... Traits> struct mangle<deque<T, Traits...>> {
  static string value() {
    return mangle_prefixed<deque<T, Traits...>>() + mangled<T>();
  }
};
template <typename T, typename... Traits> struct mangle<set<T, Traits...>> {
  static string value() {
    return mangle_prefixed<set<T, Traits...>>() + mangled<T>();
  }
};
template <typename T, typename U, typename... Traits>
struct mangle<map<T, U, Traits...>> {
  static string value() {
    return mangle_prefixed<map<T, U, Traits...>>() + kv<T, U>::value();
  }
};
template <typename T, typename... Traits>
struct mangle<unordered_set<T, Traits...>> {
  static string value() {
    return mangle_prefixed<unordered_set<T, Traits...>>() + mangled<T>();
  }
};
template <typename T, typename U, typename... Traits>
struct mangle<unordered_map<T, U, Traits...>> {
  static string value() {
    return mangle_prefixed<unordered_map<T, U, Traits...>>() +
           kv<T, U>::value();
  }
};
template <typename T, typename... Traits>
struct mangle<multiset<T, Traits...>> {
  static string value() {
    return mangle_prefixed<multiset<T, Traits...>>() + mangled<T>();
  }
};
template <typename T, typename U, typename... Traits>
struct mangle<multimap<T, U, Traits...>> {
  static string value() {
    return mangle_prefixed<multimap<T, U, Traits...>>() + kv<T, U>::value();
  }
};
template <typename T, typename... Traits>
struct mangle<unordered_multiset<T, Traits...>> {
  static string value() {
    return mangle_prefixed<unordered_multiset<T, Traits...>>() + mangled<T>();
  }
};

template <typename T, typename U, typename... Traits>
struct mangle<unordered_multimap<T, U, Traits...>> {
  static string value() {
    return mangle_prefixed<unordered_multimap<T, U, Traits...>>() +
           kv<T, U>::value();
  }
};

template <typename S, typename T, typename... Ts, typename... Bs>
struct mangle<structure<S, tuple<T, Ts...>, tuple<Bs...>>> {
  static string value() {
    return mangle_prefixed<structure<S, tuple<T, Ts...>, tuple<Bs...>>>() +
           csv<T, Ts...>::value() + "}";
  }
};

template <typename E> struct mangle<enumeration<E>> {
  static string value() {
    return mangle_prefixed<E>() + mangled<underlying_type_t<E>>() +
           accumulate(cbegin(enumeration<E>::e_to_str),
                      cend(enumeration<E>::e_to_str), string{},
                      [](auto &&s, auto e) {
                        return s += "," +
                                    to_string(static_cast<underlying_type_t<E>>(
                                        e.first));
                      }) +
           "]";
  }
};

template <typename T> struct mangle<optional<T>> {
  static string value() {
    return mangle_prefixed<optional<T>>() + mangled<T>();
  }
};

template <typename... Ts> struct mangle<variant<Ts...>> {
  static string value() {
    return mangle_prefixed<variant<Ts...>>() + csv<Ts...>::value() + "]";
  }
};

template <intmax_t N, intmax_t D> struct mangle<ratio<N, D>> {
  static string value() {
    return mangle_prefixed<ratio<N, D>>() + to_string(N) + "," + to_string(D);
  }
};

template <typename R, intmax_t N, intmax_t D>
struct mangle<chrono::duration<R, ratio<N, D>>> {
  static string value() {
    return mangle_prefixed<chrono::duration<R, ratio<N, D>>>() + mangled<R>() +
           mangled<ratio<N, D>>();
  }
};

template <typename T> struct mangle<atomic<T>> : mangle<T> {};

template <size_t N> struct mangle<bitset<N>> {
  static string value() { return mangle_prefixed<bitset<N>>() + to_string(N); }
};

#define N2W__MANGLE_SPEC(s, m, ...)                                            \
  namespace n2w {                                                              \
  template <> struct mangle<s> {                                               \
    static string value() {                                                    \
      return mangled<N2W__USING_STRUCTURE(s, m, __VA_ARGS__)>();               \
    }                                                                          \
  };                                                                           \
  }

#define N2w__MANGLE_CONV(s, c)                                                 \
  namespace n2w {                                                              \
  template <> struct mangle<s> : mangle<c> {};                                 \
  }

template <bool e = __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__>
string endianness = e ? "e" : "E";

template <typename R, typename... Ts> struct mangle<R(Ts...)> {
  static string value() {
    return mangle_prefixed<R(Ts...)>() + mangled<R>() + "=" +
           (sizeof...(Ts) ? csv<remove_cv_t<remove_reference_t<Ts>>...>::value()
                          : mangled<void *>());
  }
};
template <typename R, typename... Ts> struct mangle<R (*)(Ts...)> {
  static string value() { return mangled<R(Ts...)>(); }
};

template <typename R, typename... Ts>
string function_address(R (*f)(Ts...), uint8_t (&crypt)[sizeof(void (*)())]) {
  uintptr_t obf = 0;
  for (auto i = 0; i < sizeof(f); ++i)
    obf |= static_cast<uintptr_t>(reinterpret_cast<uint8_t *>(&f)[crypt[i]])
           << (i * 8);
  return "@" + to_string(obf) + mangled<R(Ts...)>();
}

template <typename R, typename... Ts> string function_address(R (*f)(Ts...)) {
  uint8_t scrambler[] = {0, 1, 2, 3, 4, 5, 6, 7};
  return function_address(f, scrambler);
}
}

#endif