#ifndef _NATIVE_2_WEB_SERIALIZE_HPP_
#define _NATIVE_2_WEB_SERIALIZE_HPP_

#include "native-2-web-common.hpp"

namespace n2w {
using namespace std;

template <typename T, size_t P = P, typename I>
void serialize_number(T t, I &i) {
  static_assert(is_arithmetic<T>::value, "Not an arithmetic type");
  // static_assert(is_same<uint8_t, remove_reference_t<decltype(*i)>>::value,
  // "Not dereferenceable or uint8_t iterator");

  i = copy_n(reinterpret_cast<uint8_t *>(&t), serial_size<T>, i);
  i = fill_n(i, calc_padding<P, T>(), 0);
}

template <typename T, typename I, typename J>
void serialize_numbers(uint32_t count, J j, I &i) {
  static_assert(is_arithmetic<T>::value, "Not an arithmetic type");
  // static_assert(is_same<uint8_t, remove_reference_t<decltype(*i)>>::value,
  // "Not dereferenceable or uint8_t iterator");
  // static_assert(is_same<T, remove_reference_t<decltype(*j)>>::value,
  // "Output iterator does not dereference T");

  for_each(j, j + count, [&i](const T &t) { serialize_number<T, 0>(t, i); });
  i = fill_n(i, calc_padding<P, T>(count), 0);
}

template <typename T, typename I, typename J>
void serialize_objects(uint32_t count, J j, I &i);

template <typename T, typename I, typename J>
void serialize_sequence_bounded(uint32_t count, J j, I &i, true_type) {
  serialize_numbers<T>(count, j, i);
}

template <typename T, typename I, typename J>
void serialize_sequence_bounded(uint32_t count, J j, I &i, false_type) {
  serialize_objects<T>(count, j, i);
}

template <typename T, typename I, typename J>
void serialize_sequence_bounded(uint32_t count, J j, I &i) {
  serialize_sequence_bounded<T>(count, j, i, is_arithmetic<T>{});
}

template <typename T, typename I, typename J>
void serialize_sequence(uint32_t count, J j, I &i, true_type) {
  serialize_number(count, i);
  serialize_numbers<T>(count, j, i);
}

template <typename T, typename I, typename J>
void serialize_sequence(uint32_t count, J j, I &i, false_type) {
  serialize_number(count, i);
  serialize_objects<T>(count, j, i);
}

template <typename T, typename I, typename J>
void serialize_sequence(uint32_t count, J j, I &i) {
  serialize_sequence<T>(count, j, i, is_arithmetic<T>{});
}

template <typename T, typename U, typename I, typename A>
void serialize_associative(const A &a, I &i) {
  std::vector<T> v_t;
  v_t.reserve(a.size());
  std::vector<U> v_u;
  v_u.reserve(a.size());
  transform(cbegin(a), cend(a), back_inserter(v_t),
            [](const pair<T, U> &p) { return p.first; });
  transform(cbegin(a), cend(a), back_inserter(v_u),
            [](const pair<T, U> &p) { return p.second; });
  serialize_sequence<T>(a.size(), cbegin(v_t), i);
  serialize_sequence_bounded<U>(a.size(), cbegin(v_u), i);
}

template <typename I, typename T, size_t... Is>
void serialize_heterogenous(const T &t, std::index_sequence<Is...>, I &i);

template <typename I, typename T>
auto serialize(const T &t, I &i) -> enable_if_t<is_arithmetic<T>::value> {
  serialize_number(t, i);
}
template <typename I, typename T, size_t N>
void serialize(const T (&t)[N], I &i) {
  serialize_sequence_bounded<T>(N, t, i);
}
template <typename I, typename T, size_t M, size_t N>
void serialize(const T (&t)[M][N], I &i) {
  serialize(reinterpret_cast<const T(&)[M * N]>(t), i);
}
template <typename I, typename T, size_t N>
void serialize(const array<T, N> &t, I &i) {
  serialize_sequence_bounded<T>(N, cbegin(t), i);
}
template <typename I, typename T, typename U>
void serialize(const pair<T, U> &t, I &i) {
  serialize_heterogenous(t, std::make_index_sequence<2>{}, i);
}
template <typename I, typename T, typename... Ts>
void serialize(const tuple<T, Ts...> &t, I &i) {
  serialize_heterogenous(t, std::make_index_sequence<sizeof...(Ts) + 1>{}, i);
}
template <typename I, typename T, typename... Traits>
void serialize(const basic_string<T, Traits...> &t, I &i) {
  serialize_sequence<T>(t.size(), cbegin(t), i);
}
template <typename I, typename T, typename... Traits>
void serialize(const vector<T, Traits...> &t, I &i) {
  serialize_sequence<T>(t.size(), cbegin(t), i);
}
template <typename I, typename T, typename... Traits>
void serialize(const list<T, Traits...> &t, I &i) {
  serialize_sequence<T>(t.size(), cbegin(t), i);
}
template <typename I, typename T, typename... Traits>
void serialize(const forward_list<T, Traits...> &t, I &i) {
  serialize_sequence<T>(t.size(), cbegin(t), i);
}
template <typename I, typename T, typename... Traits>
void serialize(const deque<T, Traits...> &t, I &i) {
  serialize_sequence<T>(t.size(), cbegin(t), i);
}
template <typename I, typename T, typename... Traits>
void serialize(const set<T, Traits...> &t, I &i) {
  serialize_sequence<T>(t.size(), cbegin(t), i);
}
template <typename I, typename T, typename U, typename... Traits>
void serialize(const map<T, U, Traits...> &t, I &i) {
  serialize_associative<T, U>(t, i);
}
template <typename I, typename T, typename... Traits>
void serialize(const unordered_set<T, Traits...> &t, I &i) {
  serialize_sequence<T>(t.size(), cbegin(t), i);
}
template <typename I, typename T, typename U, typename... Traits>
void serialize(const unordered_map<T, U, Traits...> &t, I &i) {
  serialize_associative<T, U>(t, i);
}
template <typename I, typename T, typename... Traits>
void serialize(const multiset<T, Traits...> &t, I &i) {
  serialize_sequence<T>(t.size(), cbegin(t), i);
}
template <typename I, typename T, typename U, typename... Traits>
void serialize(const multimap<T, U, Traits...> &t, I &i) {
  serialize_associative<T, U>(t, i);
}
template <typename I, typename T, typename... Traits>
void serialize(const unordered_multiset<T, Traits...> &t, I &i) {
  serialize_sequence<T>(t.size(), cbegin(t), i);
}
template <typename I, typename T, typename U, typename... Traits>
void serialize(const unordered_multimap<T, U, Traits...> &t, I &i) {
  serialize_associative<T, U>(t, i);
}
template <typename I, typename S, typename T, typename... Ts>
void serialize(const structure<S, T, Ts...> &t, I &i) {
  serialize_heterogenous(t, std::make_index_sequence<sizeof...(Ts) + 1>{}, i);
}

template <typename T, typename I, typename J>
void serialize_objects(uint32_t count, J j, I &i) {
  for (auto end = count, c = 0u; c < end; ++c)
    serialize(*j, i);
}

template <typename T, typename I> int serialize_index(const T &t, I &i) {
  serialize(t, i);
  return 0;
}

template <typename I, typename T, size_t... Is>
void serialize_heterogenous(const T &t, std::index_sequence<Is...>, I &i) {
  using std::get;
  using n2w::get;
  for (auto rc : {serialize_index(get<Is>(t), i)...})
    (void)rc;
}

#define SERIALIZE_SPEC(s, m)                                                   \
  namespace n2w {                                                              \
  template <typename I> void serialize(s &_s, I &i) {                          \
    USING_STRUCTURE(s, m)                                                      \
    _s_v{_s, BOOST_PP_SEQ_FOR_EACH_I(POINTER_TO_MEMBER, s, m)};                \
    serialize(_s_v, i);                                                        \
  }                                                                            \
  }
}
#endif