#ifndef _NATIVE_2_WEB_READWRITE_HPP_
#define _NATIVE_2_WEB_READWRITE_HPP_

#include "native-2-web-manglespec.hpp"
#include "native-2-web-deserialize.hpp"
#include "native-2-web-serialize.hpp"
#include <locale>

#define READ_WRITE_SPEC(s, m)                                                  \
  MANGLE_SPEC(s, m);                                                           \
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
  return f(get<N>(args)...);
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
template <typename O, typename T> O &debug_print(O &o, T &t) {
  return printer<T>::template debug_print(o, t);
}
template <typename O, size_t N> O &debug_print(O &o, const char (&t)[N]) {
  return o << mangle<basic_string<char>> << ":=\"" << t << '"';
}

template <> struct printer<bool> {
  template <size_t I = 0, typename O> static O &debug_print(O &o, bool t) {
    return o << mangle<bool> << ":='" << t << "'";
  }
};

template <size_t I = 0, typename O, typename T>
O &print_sequence(O &o, T &t, size_t count) {
  using T2 = remove_cv_t<remove_reference_t<decltype(*begin(t))>>;
  o << indent<typename O::char_type,
              I> << mangle<remove_cv_t<remove_reference_t<T>>> << ":=["
    << (is_arithmetic<T2>::value ? "" : "\n");
  size_t i = 0;
  for (auto &_t : t) {
    printer<T2>::template debug_print<I + 1>(o, _t);
    if (++i < count)
      o << ", " << (is_arithmetic<T2>::value ? "" : "\n");
  }
  o << (is_arithmetic<T2>::value ? "" : "\n" + indent<typename O::char_type, I>)
    << ']';
  return o;
}

template <size_t I = 0, typename O, typename T, size_t... Is>
O &print_heterogenous(O &o, T &t, index_sequence<Is...>) {
  bool is_num = true;
  for (auto b : {is_arithmetic<
           remove_cv_t<remove_reference_t<tuple_element_t<Is, T>>>>::value...})
    is_num &= b;

  o << indent<typename O::char_type,
              I> << mangle<remove_cv_t<remove_reference_t<T>>> << ":={"
    << (is_num ? "" : "\n");
  for (auto &_o :
       {&(printer<remove_cv_t<remove_reference_t<tuple_element_t<Is, T>>>>::
              template debug_print<Is + 1>(o, get<Is>(t))
          << (Is < sizeof...(Is) ? string{", "} + (is_num ? "" : "\n")
                                 : ""))...})
    (void)_o;
  o << (is_num ? "" : "\n" + indent<typename O::char_type, I>) << '}';
  return o;
}

template <typename T> struct printer {
  template <size_t I = 0, typename O>
  static auto debug_print(O &o, T t)
      -> enable_if_t<is_arithmetic<T>::value, O &> {
    return o << mangle<remove_cv_t<remove_reference_t<T>>> << ":='" << t << "'";
  }
};
template <typename T, size_t N> struct printer<T[N]> {
  template <size_t I = 0, typename O> static O &debug_print(O &o, T (&t)[N]) {
    return print_sequence<I>(o, t, N);
  }
};
template <typename T, size_t N> struct printer<array<T, N>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, array<T, N> &t) {
    return print_sequence<I>(o, t, N);
  }
};
template <typename T, typename U> struct printer<pair<T, U>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, pair<T, U> &t) {
    return print_heterogenous<I>(o, t, make_index_sequence<2>{});
  }
};
template <typename T, typename... Ts> struct printer<tuple<T, Ts...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, tuple<T, Ts...> &t) {
    return print_heterogenous<I>(o, t,
                                 make_index_sequence<sizeof...(Ts) + 1>{});
  }
};
template <size_t N> struct printer<const char[N]> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, char (&t)[N]) {
    return o << mangle<basic_string<char>> << ":=\"" << t << '"';
  }
};
template <typename T, typename... Traits>
struct printer<basic_string<T, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const basic_string<T, Traits...> &t) {
    struct cvt : codecvt<T, typename O::char_type, mbstate_t> {};
    wstring_convert<cvt, T> cvter;
    return o << mangle<basic_string<T, Traits...>> << ":=\""
             << cvter.to_bytes(t) << '"';
  }
};
template <typename T, typename... Traits> struct printer<vector<T, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, vector<T, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename... Traits> struct printer<list<T, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, list<T, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename... Traits>
struct printer<forward_list<T, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, forward_list<T, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename... Traits> struct printer<deque<T, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, deque<T, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename... Traits> struct printer<set<T, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, set<T, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename U, typename... Traits>
struct printer<map<T, U, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, map<T, U, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename... Traits>
struct printer<unordered_set<T, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, unordered_set<T, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename U, typename... Traits>
struct printer<unordered_map<T, U, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, unordered_map<T, U, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename... Traits>
struct printer<multiset<T, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, multiset<T, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename U, typename... Traits>
struct printer<multimap<T, U, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, multimap<T, U, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename... Traits>
struct printer<unordered_multiset<T, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, unordered_multiset<T, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename U, typename... Traits>
struct printer<unordered_multimap<T, U, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, unordered_multimap<T, U, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename S, typename T, typename... Ts>
struct printer<structure<S, T, Ts...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, structure<S, T, Ts...> &t) {
    return print_heterogenous<I>(o, t,
                                 make_index_sequence<sizeof...(Ts) + 1>{});
  }
};

#define DEBUG_SPEC(s, m)                                                       \
  namespace n2w {                                                              \
  template <> struct printer<s> {                                              \
    template <size_t I = 0, typename O> static O &debug_print(O &o, s &_s) {   \
      CONSTRUCTOR(s, m, _s);                                                   \
      return printer<decltype(_s_v)>::template debug_print<I>(o, _s_v);        \
    }                                                                          \
  };                                                                           \
  }
}

#endif