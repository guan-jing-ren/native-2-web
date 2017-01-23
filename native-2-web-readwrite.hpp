#ifndef _NATIVE_2_WEB_READWRITE_HPP_
#define _NATIVE_2_WEB_READWRITE_HPP_

#include "native-2-web-manglespec.hpp"
#include "native-2-web-deserialize.hpp"
#include "native-2-web-serialize.hpp"
#include <locale>

#define READ_WRITE_SPEC(s, m)                                                  \
  MANGLE_SPEC(s, m);                                                           \
  SPECIALIZE_STRUCTURE(s, m);                                                  \
  SERIALIZE_SPEC(s, m);                                                        \
  DESERIALIZE_SPEC(s, m);                                                      \
  DEBUG_SPEC(s, m);

namespace n2w {
using namespace std;

namespace {
template <typename I> int deserialize_arg(I &i) { return 0; }
template <typename I, typename T> int deserialize_arg(I &i, T &t) {
  deserialize(i, t);
  return 0;
}
}

template <typename I, typename J, typename R, typename... Ts, size_t... N>
R execute(I &i, J &j, R (*f)(Ts...), index_sequence<N...>) {
  tuple<remove_cv_t<remove_reference_t<Ts>>...> args;
  for (auto rc : initializer_list<int>{deserialize_arg(j, get<N>(args))...})
    (void)rc;
  return f(forward<tuple_element_t<N, decltype(args)>>(get<N>(args))...);
}

template <typename I, typename J, typename R, typename... Ts>
void execute(I &i, J &j, R (*f)(Ts...)) {
  auto rv = execute(i, j, f, make_index_sequence<sizeof...(Ts)>{});
  serialize(rv, i);
}
template <typename I, typename J, typename... Ts>
void execute(I &i, J &j, void (*f)(Ts...)) {
  execute(i, j, f, make_index_sequence<sizeof...(Ts)>{});
}

template <typename C> constexpr char indent_char = 0;
template <> constexpr char indent_char<char> = ' ';
template <> constexpr wchar_t indent_char<wchar_t> = L' ';
template <> constexpr char16_t indent_char<char16_t> = u' ';
template <> constexpr char32_t indent_char<char32_t> = U' ';

constexpr size_t tab_size = 2;
template <typename C, size_t I = 0>
const basic_string<C> indent = basic_string<C>(I *tab_size, indent_char<C>);

template <typename T> struct printer;
template <typename O, typename T> O &debug_print(O &o, const T &t) {
  return printer<T>::template debug_print(o, t);
}
template <typename O, size_t N> O &debug_print(O &o, const char (&t)[N]) {
  return o << mangled<basic_string<char>>() << ":=\"" << t << '"';
}

template <> struct printer<bool> {
  template <size_t I = 0, typename O> static O &debug_print(O &o, bool t) {
    return o << indent<typename O::char_type, I> << mangled<bool>() << ":='"
             << t << "'";
  }
};

template <size_t I = 0, typename O, typename T>
O &print_sequence(O &o, const T &t, size_t count) {
  using T2 = remove_cv_t<remove_reference_t<decltype(*begin(t))>>;
  o << indent<typename O::char_type,
              I> << mangled<remove_cv_t<remove_reference_t<T>>>()
    << ":=[" << (is_arithmetic<T2>::value || !count ? "" : "\n");
  size_t i = 0;
  if (count)
    for (auto &_t : t) {
      printer<T2>::template debug_print<is_arithmetic<T2>::value ? 0u : I + 1>(
          o, _t);
      if (++i < count)
        o << ", " << (is_arithmetic<T2>::value ? "" : "\n");
    }
  o << (is_arithmetic<T2>::value || !count
            ? ""
            : "\n" + indent<typename O::char_type, I>)
    << ']';
  return o;
}

template <typename T, size_t... Is>
constexpr bool is_all_num(index_sequence<Is...>) {
  bool is_num = true;
  for (auto b : {is_arithmetic<
           remove_cv_t<remove_reference_t<element_t<Is, T>>>>::value...})
    is_num &= b;
  return is_num;
}

template <size_t I = 0, typename O, typename T, size_t... Is>
O &print_heterogenous(O &o, T &t, index_sequence<Is...>) {
  constexpr bool is_num = is_all_num<T>(index_sequence<Is...>{});
  o << indent<typename O::char_type,
              I> << mangled<remove_cv_t<remove_reference_t<T>>>()
    << ":={" << (is_num ? "" : "\n");
  for (auto &_o :
       {&(printer<remove_cv_t<remove_reference_t<element_t<Is, T>>>>::
              template debug_print<(is_num ? 0u : (I + 1))>(o, get<Is>(t))
          << "\t`" << name<Is>(t) << '`' << at<Is>(t)
          << ((Is + 1) < sizeof...(Is) ? string{", "} + (is_num ? "" : "\n")
                                       : ""))...})
    (void)_o;
  o << (is_num ? "" : "\n" + indent<typename O::char_type, I>) << '}';
  return o;
}

template <typename T> struct printer {
  template <size_t I = 0, typename O>
  static auto debug_print(O &o, T t)
      -> enable_if_t<is_arithmetic<T>::value, O &> {
    return o << indent<typename O::char_type,
                       I> << mangled<remove_cv_t<remove_reference_t<T>>>()
             << ":='" << t << "'";
  }
};
template <typename T, size_t N> struct printer<T[N]> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const T (&t)[N]) {
    return print_sequence<I>(o, t, N);
  }
};
template <typename T, size_t N> struct printer<array<T, N>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const array<T, N> &t) {
    return print_sequence<I>(o, t, N);
  }
};
template <typename T, typename U> struct printer<pair<T, U>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const pair<T, U> &t) {
    return print_heterogenous<I>(o, t, make_index_sequence<2>{});
  }
};
template <typename T, typename... Ts> struct printer<tuple<T, Ts...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const tuple<T, Ts...> &t) {
    return print_heterogenous<I>(o, t,
                                 make_index_sequence<sizeof...(Ts) + 1>{});
  }
};
template <size_t N> struct printer<char[N]> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const char (&t)[N]) {
    return o << mangled<basic_string<char>>() << ":=\"" << t << '"';
  }
};
template <typename T, typename... Traits>
struct printer<basic_string<T, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const basic_string<T, Traits...> &t) {
    struct cvt : codecvt<T, typename O::char_type, mbstate_t> {};
    wstring_convert<cvt, T> cvter;
    return o << mangled<basic_string<T, Traits...>>() << ":=\""
             << cvter.to_bytes(t) << '"';
  }
};
template <typename T, typename... Traits> struct printer<vector<T, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const vector<T, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename... Traits> struct printer<list<T, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const list<T, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename... Traits>
struct printer<forward_list<T, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const forward_list<T, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename... Traits> struct printer<deque<T, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const deque<T, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename... Traits> struct printer<set<T, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const set<T, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename U, typename... Traits>
struct printer<map<T, U, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const map<T, U, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename... Traits>
struct printer<unordered_set<T, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const unordered_set<T, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename U, typename... Traits>
struct printer<unordered_map<T, U, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const unordered_map<T, U, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename... Traits>
struct printer<multiset<T, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const multiset<T, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename U, typename... Traits>
struct printer<multimap<T, U, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const multimap<T, U, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename... Traits>
struct printer<unordered_multiset<T, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const unordered_multiset<T, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename U, typename... Traits>
struct printer<unordered_multimap<T, U, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const unordered_multimap<T, U, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename S, typename T, typename... Ts>
struct printer<structure<S, T, Ts...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const structure<S, T, Ts...> &t) {
    return print_heterogenous<I>(o, t, make_index_sequence<sizeof...(Ts) + 1>{})
           << "\t`" << *begin(t.names) << '`';
  }
};

template <typename T, size_t V = 5, size_t N = V, size_t O = 0, size_t S = 1>
struct filler {
  size_t iteration = O;
  T t;
  template <size_t V2, size_t N2, size_t O2, size_t S2>
  filler(filler<T, V2, N2, O2, S2> f) {}
  filler() = default;
  filler(const filler &) = default;
  filler(filler &&) = default;
  filler &operator=(const filler &) = default;
  filler &operator=(filler &&) = default;
  ~filler() = default;

  filler &operator++() {
    iteration += S;
    iteration %= (V * S);
    return *this;
  }

  filler operator++(int) {
    auto current = *this;
    ++*this;
    return current;
  }

  T operator*() { return t; }

  T operator()() { return *((*this)++); }
};

// template <size_t V, size_t N, size_t O, size_t S>
// bool filler<bool, V, N, O, S>::operator()() {
//   return t;
// }
// template <size_t V, size_t N, size_t O, size_t S>
// char filler<char, V, N, O, S>::operator()() {
//   return t;
// }
// template <size_t V, size_t N, size_t O, size_t S>
// wchar_t filler<wchar_t, V, N, O, S>::operator()() {
//   return t;
// }
// template <size_t V, size_t N, size_t O, size_t S>
// char16_t filler<char16_t, V, N, O, S>::operator()() {
//   return t;
// }
// template <size_t V, size_t N, size_t O, size_t S>
// char32_t filler<char32_t, V, N, O, S>::operator()() {
//   return t;
// }
// template <size_t V, size_t N, size_t O, size_t S> T filler<int8_t,size_t V,
// size_t N, size_t O, size_t S>::operator()() {return t;}
// template <size_t V, size_t N, size_t O, size_t S> T filler<int16_t,size_t V,
// size_t N, size_t O, size_t S>::operator()() {return t;}
// template <size_t V, size_t N, size_t O, size_t S> T filler<int32_t,size_t V,
// size_t N, size_t O, size_t S>::operator()() {return t;}
// template <size_t V, size_t N, size_t O, size_t S> T filler<int64_t,size_t V,
// size_t N, size_t O, size_t S>::operator()() {return t;}
// template <size_t V, size_t N, size_t O, size_t S> T filler<uint8_t,size_t V,
// size_t N, size_t O, size_t S>::operator()() {return t;}
// template <size_t V, size_t N, size_t O, size_t S> T filler<uint16_t,size_t V,
// size_t N, size_t O, size_t S>::operator()() {return t;}
// template <size_t V, size_t N, size_t O, size_t S> T filler<uint32_t,size_t V,
// size_t N, size_t O, size_t S>::operator()() {return t;}
// template <size_t V, size_t N, size_t O, size_t S> T filler<uint32_t,size_t V,
// size_t N, size_t O, size_t S>::operator()() {return t;}
// template <size_t V, size_t N, size_t O, size_t S> T filler<float,size_t V,
// size_t N, size_t O, size_t S>::operator()() {return t;}
// template <size_t V, size_t N, size_t O, size_t S> T filler<double,size_t V,
// size_t N, size_t O, size_t S>::operator()() {return t;}
// template <size_t V, size_t N, size_t O, size_t S> T filler<long double,size_t
// V, size_t N, size_t O, size_t S>::operator()() {return t;}

#define DEBUG_SPEC(s, m)                                                       \
  namespace n2w {                                                              \
  template <> struct printer<s> {                                              \
    template <size_t I = 0, typename O>                                        \
    static O &debug_print(O &o, const s &_s) {                                 \
      CONSTRUCTOR(s, m, _s);                                                   \
      return printer<decltype(_s_v)>::template debug_print<I>(o, _s_v);        \
    }                                                                          \
  };                                                                           \
  }
}

#endif