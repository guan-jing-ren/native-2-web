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

template <typename T> struct traverser;

template <typename T> struct traverser {
  static auto traverse(const T &t) -> enable_if_t<is_arithmetic<T>::value> {}
};
template <typename T, size_t N> struct traverser<T[N]> {
  static void traverse(const T (&t)[N]) {}
};
template <typename T, size_t M, size_t N> struct traverser<T[M][N]> {
  static void traverse(const T (&t)[M][N]) {}
};
template <typename T, size_t N> struct traverser<array<T, N>> {
  static void traverse(const array<T, N> &t) {}
};
template <typename T, typename U> struct traverser<pair<T, U>> {
  static void traverse(const pair<T, U> &t) {}
};
template <typename T, typename... Ts> struct traverser<tuple<T, Ts...>> {
  static void traverse(const tuple<T, Ts...> &t) {}
};
template <typename T, typename... Traits>
struct traverser<basic_string<T, Traits...>> {
  static void traverse(const basic_string<T, Traits...> &t) {}
};
template <typename T, typename... Traits>
struct traverser<vector<T, Traits...>> {
  static void traverse(const vector<T, Traits...> &t) {}
};
template <typename T, typename... Traits> struct traverser<list<T, Traits...>> {
  static void traverse(const list<T, Traits...> &t) {}
};
template <typename T, typename... Traits>
struct traverser<forward_list<T, Traits...>> {
  static void traverse(const forward_list<T, Traits...> &t) {}
};
template <typename T, typename... Traits>
struct traverser<deque<T, Traits...>> {
  static void traverse(const deque<T, Traits...> &t) {}
};
template <typename T, typename... Traits> struct traverser<set<T, Traits...>> {
  static void traverse(const set<T, Traits...> &t) {}
};
template <typename T, typename U, typename... Traits>
struct traverser<map<T, U, Traits...>> {
  static void traverse(const map<T, U, Traits...> &t) {}
};
template <typename T, typename... Traits>
struct traverser<unordered_set<T, Traits...>> {
  static void traverse(const unordered_set<T, Traits...> &t) {}
};
template <typename T, typename U, typename... Traits>
struct traverser<unordered_map<T, U, Traits...>> {
  static void traverse(const unordered_map<T, U, Traits...> &t) {}
};
template <typename T, typename... Traits>
struct traverser<multiset<T, Traits...>> {
  static void traverse(const multiset<T, Traits...> &t) {}
};
template <typename T, typename U, typename... Traits>
struct traverser<multimap<T, U, Traits...>> {
  static void traverse(const multimap<T, U, Traits...> &t) {}
};
template <typename T, typename... Traits>
struct traverser<unordered_multiset<T, Traits...>> {
  static void traverse(const unordered_multiset<T, Traits...> &t) {}
};
template <typename T, typename U, typename... Traits>
struct traverser<unordered_multimap<T, U, Traits...>> {
  static void traverse(const unordered_multimap<T, U, Traits...> &t) {}
};
template <typename S, typename T, typename... Ts>
struct traverser<structure<S, T, Ts...>> {
  static void traverse(const structure<S, T, Ts...> &t) {}
};
}

#endif