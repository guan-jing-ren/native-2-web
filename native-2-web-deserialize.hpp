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

template <typename I, typename T, size_t N>
constexpr auto deserialize(I i, T (&)[N]) {}
template <typename I, typename T, size_t N>
constexpr auto deserialize(I i, array<T, N> &) {}
template <typename I, typename T, typename U>
constexpr auto deserialize(I i, pair<T, U> &) {}
template <typename I, typename T, typename... Ts>
constexpr auto deserialize(I i, tuple<T, Ts...> &) {}
template <typename I, typename T, typename... Traits>
constexpr auto deserialize(I i, basic_string<T, Traits...> &) {}
template <typename I, typename T, typename... Traits>
constexpr auto deserialize(I i, vector<T, Traits...> &) {}
template <typename I, typename T, typename... Traits>
constexpr auto deserialize(I i, list<T, Traits...> &) {}
template <typename I, typename T, typename... Traits>
constexpr auto deserialize(I i, forward_list<T, Traits...> &) {}
template <typename I, typename T, typename... Traits>
constexpr auto deserialize(I i, deque<T, Traits...> &) {}
template <typename I, typename T, typename... Traits>
constexpr auto deserialize(I i, set<T, Traits...> &) {}
template <typename I, typename T, typename U, typename... Traits>
constexpr auto deserialize(I i, map<T, U, Traits...> &) {}
template <typename I, typename T, typename... Traits>
constexpr auto deserialize(I i, unordered_set<T, Traits...> &) {}
template <typename I, typename T, typename U, typename... Traits>
constexpr auto deserialize(I i, unordered_map<T, U, Traits...> &) {}
template <typename I, typename T, typename... Traits>
constexpr auto deserialize(I i, multiset<T, Traits...> &) {}
template <typename I, typename T, typename U, typename... Traits>
constexpr auto deserialize(I i, multimap<T, U, Traits...> &) {}
template <typename I, typename T, typename... Traits>
constexpr auto deserialize(I i, unordered_multiset<T, Traits...> &) {}
template <typename I, typename T, typename U, typename... Traits>
constexpr auto deserialize(I i, unordered_multimap<T, U, Traits...> &) {}
template <typename I, typename... Ts>
const auto deserialize(I i, structure<Ts...> &) {}



template <typename T, size_t P, typename I, typename J>
auto deserialize_objects(uint32_t count, I &i, J &j) {
  generate_n(j, count, [&i]() {
    T t;
    deserialize<T, P>(t);
    return t;
  });
}
}
#endif