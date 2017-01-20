#ifndef _NATIVE_2_WEB_READWRITE_HPP_
#define _NATIVE_2_WEB_READWRITE_HPP_

#include "native-2-web-manglespec.hpp"
#include "native-2-web-deserialize.hpp"
#include "native-2-web-serialize.hpp"

#define READ_WRITE_SPEC(s, m)                                                  \
  MANGLE_SPEC(s, m);                                                           \
  SERIALIZE_SPEC(s, m);                                                        \
  DESERIALIZE_SPEC(s, m);

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

template <size_t I = 0, typename O> O &debug_print(O &o, bool t) {
  return o << mangle_prefix<bool> << ":='" << t << "'";
}
template <size_t I = 0, typename O, typename T>
auto debug_print(O &o, T t) -> enable_if_t<is_arithmetic<T>::value, O &> {
  return o << mangle_prefix<T> << ":='" << t << "'";
}
template <size_t I = 0, typename O, typename T, size_t N>
O &debug_print(O &o, T (&t)[N]) {
  o << indent<typename O::char_type, I> << mangle_prefix<T[N]> << ":=["
    << (is_arithmetic<T>::value ? "" : "\n");
  size_t i = 0;
  for (const auto &_t : t) {
    debug_print<I + 1>(o, _t);
    if (++i < N)
      o << ", " << (is_arithmetic<T>::value ? "" : "\n");
  }
  o << (is_arithmetic<T>::value ? "" : "\n" + indent<typename O::char_type, I>)
    << ']';
  return o;
}
template <size_t I = 0, typename O, typename T, size_t N>
O &debug_print(O &o, array<T, N> &t) {
  return o;
}
template <size_t I = 0, typename O, typename T, typename U>
O &debug_print(O &o, pair<T, U> &t) {
  return o;
}
template <size_t I = 0, typename O, typename T, typename... Ts>
O &debug_print(O &o, tuple<T, Ts...> &t) {
  return o;
}
template <size_t I = 0, typename O, typename T, typename... Traits>
O &debug_print(O &o, basic_string<T, Traits...> &t) {
  return o;
}
template <size_t I = 0, typename O, typename T, typename... Traits>
O &debug_print(O &o, vector<T, Traits...> &t) {
  return o;
}
template <size_t I = 0, typename O, typename T, typename... Traits>
O &debug_print(O &o, list<T, Traits...> &t) {
  return o;
}
template <size_t I = 0, typename O, typename T, typename... Traits>
O &debug_print(O &o, forward_list<T, Traits...> &t) {
  return o;
}
template <size_t I = 0, typename O, typename T, typename... Traits>
O &debug_print(O &o, deque<T, Traits...> &t) {
  return o;
}
template <size_t I = 0, typename O, typename T, typename... Traits>
O &debug_print(O &o, set<T, Traits...> &t) {
  return o;
}
template <size_t I = 0, typename O, typename T, typename U, typename... Traits>
O &debug_print(O &o, map<T, U, Traits...> &t) {
  return o;
}
template <size_t I = 0, typename O, typename T, typename... Traits>
O &debug_print(O &o, unordered_set<T, Traits...> &t) {
  return o;
}
template <size_t I = 0, typename O, typename T, typename U, typename... Traits>
O &debug_print(O &o, unordered_map<T, U, Traits...> &t) {
  return o;
}
template <size_t I = 0, typename O, typename T, typename... Traits>
O &debug_print(O &o, multiset<T, Traits...> &t) {
  return o;
}
template <size_t I = 0, typename O, typename T, typename U, typename... Traits>
O &debug_print(O &o, multimap<T, U, Traits...> &t) {
  return o;
}
template <size_t I = 0, typename O, typename T, typename... Traits>
O &debug_print(O &o, unordered_multiset<T, Traits...> &t) {
  return o;
}
template <size_t I = 0, typename O, typename T, typename U, typename... Traits>
O &debug_print(O &o, unordered_multimap<T, U, Traits...> &t) {
  return o;
}
template <size_t I = 0, typename O, typename S, typename T, typename... Ts>
O &debug_print(O &o, structure<S, T, Ts...> &t) {
  return o;
}
}

#endif