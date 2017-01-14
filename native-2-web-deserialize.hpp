#ifndef _NATIVE_2_WEB_DESERIALIZE_HPP_
#define _NATIVE_2_WEB_DESERIALIZE_HPP_

#include "native-2-web-common.hpp"
#include <algorithm>

namespace n2w {
using namespace std;
template <typename T, size_t P = sizeof(T), typename I>
T deserialize_number(I &i) {
  static_assert(is_arithmetic<T>::value, "Not an arithmetic type");
  static_assert(is_same<uint8_t, remove_reference_t<decltype(*i)>>::value,
                "Not dereferenceable or uint8_t iterator");

  T t = 0;
  copy_n(i, sizeof(t), reinterpret_cast<uint8_t *>(&t));
  i += sizeof(t) + calc_padding<P, T>();
  return t;
}

template <typename T, size_t P = sizeof(T), typename I, typename J>
void deserialize_numbers(uint32_t count, I &i, J &j) {
  static_assert(is_arithmetic<T>::value, "Not an arithmetic type");
  static_assert(is_same<uint8_t, remove_reference_t<decltype(*i)>>::value,
                "Not dereferenceable or uint8_t iterator");
  static_assert(is_same<T, remove_reference_t<decltype(*j)>>::value,
                "Output iterator does not dereference T");

  generate_n(j, count, [&i]() { return deserialize_number<T>(i); });
  i += calc_padding<P, T>(count);
}

template <typename T, size_t P, typename I, typename J>
auto deserialize_objects(uint32_t count, I &i, J &j);

template <typename T, size_t P = sizeof(T), typename I, typename J>
void deserialize_sequence(uint32_t count, I &i, J &j, true_type) {
  deserialize_numbers<T, P>(count, i, j);
}

template <typename T, size_t P = sizeof(T), typename I, typename J>
void deserialize_sequence(uint32_t count, I &i, J &j, false_type) {
  deserialize_objects<T, P>(count, i, j);
}

template <typename T, size_t P = sizeof(T), typename I, typename J>
void deserialize_sequence(I &i, J &j) {
  auto count = deserialize_number<T, P>(i);
  deserialize_sequence<T, P>(count, i, j, is_arithmetic<T>{});
}

template <typename T, typename U, size_t P = sizeof(T), typename I, typename A>
void deserialize_associative(I &i, A &a) {
  vector<T> v_t;
  vector<U> v_u;
  auto count = deserialize_number<T, P>(i);
  deserialize_sequence<T, P>(count, i, back_inserter(v_t), is_arithmetic<T>{});
  deserialize_sequence<T, P>(count, i, back_inserter(v_u), is_arithmetic<T>{});
  inner_product(begin(v_t), end(v_t), begin(v_u), end(v_u), 0,
                [&a](int, pair<T, U> &p) {
                  a.emplace(p);
                  return 0;
                },
                [](T &t, U &u) { return make_pair(t, u); });
}

template <size_t P, typename I, typename T, size_t... Is>
void deserialize_heterogenous(I &i, T &t, std::index_sequence<Is...>);

template <typename I, typename T, size_t P = sizeof(T), size_t N>
constexpr auto deserialize(I &i, T (&t)[N]) {
  deserialize_sequence<T, P>(N, i, t, is_arithmetic<T>{});
}
template <typename I, typename T, size_t P = sizeof(T), size_t N>
constexpr auto deserialize(I &i, array<T, N> &t) {
  deserialize_sequence<T, P>(N, i, begin(t), is_arithmetic<T>{});
}
template <typename I, typename T, size_t P = sizeof(T), typename U>
constexpr auto deserialize(I &i, pair<T, U> &t) {
  deserialize_heterogenous<P>(i, t, std::make_index_sequence<2>{});
}
template <typename I, typename T, size_t P = sizeof(T), typename... Ts>
constexpr auto deserialize(I &i, tuple<T, Ts...> &t) {
  deserialize_heterogenous<P>(i, t,
                              std::make_index_sequence<sizeof...(Ts) + 1>{});
}
template <typename I, typename T, size_t P = sizeof(T), typename... Traits>
constexpr auto deserialize(I &i, basic_string<T, Traits...> &t) {
  deserialize_sequence<T, P>(i, back_inserter(t));
}
template <typename I, typename T, size_t P = sizeof(T), typename... Traits>
constexpr auto deserialize(I &i, vector<T, Traits...> &t) {
  deserialize_sequence<T, P>(i, back_inserter(t));
}
template <typename I, typename T, size_t P = sizeof(T), typename... Traits>
constexpr auto deserialize(I &i, list<T, Traits...> &t) {
  deserialize_sequence<T, P>(i, back_inserter(t));
}
template <typename I, typename T, size_t P = sizeof(T), typename... Traits>
constexpr auto deserialize(I &i, forward_list<T, Traits...> &t) {
  deserialize_sequence<T, P>(i, back_inserter(t));
}
template <typename I, typename T, size_t P = sizeof(T), typename... Traits>
constexpr auto deserialize(I &i, deque<T, Traits...> &t) {
  deserialize_sequence<T, P>(i, back_inserter(t));
}
template <typename I, typename T, size_t P = sizeof(T), typename... Traits>
constexpr auto deserialize(I &i, set<T, Traits...> &t) {
  deserialize_sequence<T, P>(i, inserter(t));
}
template <typename I, typename T, size_t P = sizeof(T), typename U,
          typename... Traits>
constexpr auto deserialize(I &i, map<T, U, Traits...> &t) {
  deserialize_associative<T, U, P>(i, t);
}
template <typename I, typename T, size_t P = sizeof(T), typename... Traits>
constexpr auto deserialize(I &i, unordered_set<T, Traits...> &t) {
  deserialize_sequence<T, P>(i, inserter(t));
}
template <typename I, typename T, size_t P = sizeof(T), typename U,
          typename... Traits>
constexpr auto deserialize(I &i, unordered_map<T, U, Traits...> &t) {
  deserialize_associative<T, U, P>(i, t);
}
template <typename I, typename T, size_t P = sizeof(T), typename... Traits>
constexpr auto deserialize(I &i, multiset<T, Traits...> &t) {
  deserialize_sequence<T, P>(i, inserter(t));
}
template <typename I, typename T, size_t P = sizeof(T), typename U,
          typename... Traits>
constexpr auto deserialize(I &i, multimap<T, U, Traits...> &t) {
  deserialize_associative<T, U, P>(i, t);
}
template <typename I, typename T, size_t P = sizeof(T), typename... Traits>
constexpr auto deserialize(I &i, unordered_multiset<T, Traits...> &t) {
  deserialize_sequence<T, P>(i, inserter(t));
}
template <typename I, typename T, size_t P = sizeof(T), typename U,
          typename... Traits>
constexpr auto deserialize(I &i, unordered_multimap<T, U, Traits...> &t) {
  deserialize_associative<T, U, P>(i, t);
}
template <typename I, size_t P, typename... Ts>
const auto deserialize(I &i, structure<Ts...> &) {}

template <typename T, size_t P = sizeof(T), typename I, typename J>
auto deserialize_objects(uint32_t count, I &i, J &j) {
  generate_n(j, count, [&i]() {
    T t;
    deserialize<T, P>(t);
    return t;
  });
}

template <size_t P, typename T, typename I> int deserialize_index(I &i, T &t) {
  deserialize<T, P>(i, t);
  return 0;
}

template <size_t P, typename I, typename T, size_t... Is>
void deserialize_heterogenous(I &i, T &t, std::index_sequence<Is...>) {
  using std::get;
  for (auto rc : {deserialize_index<P>(i, get<Is>(t))...})
    (void)rc;
}
}
#endif