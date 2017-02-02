#ifndef _NATIVE_2_WEB_COMMON_HPP_
#define _NATIVE_2_WEB_COMMON_HPP_

#include <cstdint>
#include <bitset>
#include <array>
#include <tuple>
#include <string>
#include <vector>
#include <list>
#include <forward_list>
#include <deque>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <boost/preprocessor.hpp>

namespace std {
template <typename T> bool less_unordered(const T &l, const T &r) {
  vector<add_pointer_t<add_const_t<remove_reference_t<decltype(*begin(l))>>>>
      l_v;
  auto r_v = l_v;
  transform(begin(l), end(l), back_inserter(l_v),
            [](const auto &l) { return &l; });
  transform(begin(r), end(r), back_inserter(r_v),
            [](const auto &r) { return &r; });
  sort(begin(l_v), end(l_v),
       [](const auto &l, const auto &r) { return *l < *r; });
  sort(begin(r_v), end(r_v),
       [](const auto &l, const auto &r) { return *l < *r; });
  return l_v < r_v;
}

template <typename T, typename... Traits>
bool operator<(const unordered_set<T, Traits...> &l,
               const unordered_set<T, Traits...> &r) {
  return less_unordered(l, r);
}
template <typename T, typename... Traits>
bool operator<(const unordered_map<T, Traits...> &l,
               const unordered_map<T, Traits...> &r) {
  return less_unordered(l, r);
}
template <typename T, typename... Traits>
bool operator<(const unordered_multiset<T, Traits...> &l,
               const unordered_multiset<T, Traits...> &r) {
  return less_unordered(l, r);
}
template <typename T, typename... Traits>
bool operator<(const unordered_multimap<T, Traits...> &l,
               const unordered_multimap<T, Traits...> &r) {
  return less_unordered(l, r);
}
}

template <typename T> constexpr T reverse_endian(T t) {
  std::reverse(reinterpret_cast<std::uint8_t *>(&t),
          reinterpret_cast<std::uint8_t *>(&t) + sizeof(T));
  return t;
}

template <typename T> constexpr auto serial_size = sizeof(T);
// template <> constexpr auto serial_size<void> = sizeof("void");
// template <> constexpr auto serial_size<bool> = sizeof(std::uint8_t);
// template <> constexpr auto serial_size<wchar_t> = sizeof(char32_t);
constexpr size_t P = serial_size<double>;

template <std::size_t P, typename T>
constexpr auto calc_padding(std::size_t count = 0) {
  return P == 0 ? 0 : (P - ((count * serial_size<T>) % P)) % P;
}

namespace n2w {
template <typename S, typename T, typename... Ts> struct structure {
  const volatile union {
    S *const volatile s_write;
    const S *const volatile s_read;
  };
  static const std::tuple<T S::*, Ts S::*...> members;
  static const char *names[];
  structure(S *s) : s_write(s) {}
  structure(const S *s) : s_read(s) {}
  structure(std::nullptr_t) = delete;
  structure(int) = delete;
  structure(size_t) = delete;
  structure() = delete;
  structure(const structure &) = delete;
  structure(structure &&) = delete;
  structure &operator=(const structure &) = delete;
  structure &operator=(structure &&) = delete;
  structure &operator=(const structure &&) = delete;
};

template <size_t N, typename S, typename T, typename... Ts>
auto &get(const structure<S, T, Ts...> &s) {
  return s.s_read->*get<N>(s.members);
}

template <size_t N, typename S, typename T, typename... Ts>
auto &get(structure<S, T, Ts...> &s) {
  return s.s_write->*get<N>(s.members);
}

template <typename S, typename T, typename... Ts, size_t... Is>
bool memberwise_equality(const structure<S, T, Ts...> &l,
                         const structure<S, T, Ts...> &r,
                         std::index_sequence<Is...>) {
  if (l.s_read == r.s_read)
    return true;

  bool equal = true;
  for (auto rc : {(equal = equal && get<Is>(l) == get<Is>(r))...})
    if (!equal)
      return equal;
  return equal;
}
template <typename S, typename T, typename... Ts>
bool operator==(const structure<S, T, Ts...> &l,
                const structure<S, T, Ts...> &r) {
  return memberwise_equality(l, r,
                             std::make_index_sequence<sizeof...(Ts) + 1>{});
}
template <typename S, typename T, typename... Ts>
bool operator!=(const structure<S, T, Ts...> &l,
                const structure<S, T, Ts...> &r) {
  return !(l == r);
}

template <typename S, typename T, typename... Ts, size_t... Is>
bool memberwise_less(const structure<S, T, Ts...> &l,
                     const structure<S, T, Ts...> &r,
                     std::index_sequence<Is...>) {
  if (l.s_read == r.s_read)
    return false;

  std::pair<bool, bool> lesser_greater = std::make_pair(false, false);
  for (auto rc :
       {(lesser_greater = std::make_pair(
             !lesser_greater.second &&
                 (lesser_greater.first || get<Is>(l) < get<Is>(r)),
             !lesser_greater.first &&
                 (lesser_greater.second || get<Is>(r) < get<Is>(l))))...}) {
    if (lesser_greater.first)
      return true;
    if (lesser_greater.second)
      return false;
  }
  return false;
}
template <typename S, typename T, typename... Ts>
bool operator<(const structure<S, T, Ts...> &l,
               const structure<S, T, Ts...> &r) {
  return memberwise_less(l, r, std::make_index_sequence<sizeof...(Ts) + 1>{});
}

template <typename S, typename T, typename... Ts>
bool operator>(const structure<S, T, Ts...> &l,
               const structure<S, T, Ts...> &r) {
  return r < l;
};

template <typename S, typename T, typename... Ts>
bool operator<=(const structure<S, T, Ts...> &l,
                const structure<S, T, Ts...> &r) {
  return !(r < l);
};

template <typename S, typename T, typename... Ts>
bool operator>=(const structure<S, T, Ts...> &l,
                const structure<S, T, Ts...> &r) {
  return !(l < r);
};

template <size_t N, typename T, typename U>
const char *at(const std::pair<T, U> &) {
  return "";
}

template <size_t N, typename T, typename... Ts>
const char *at(const std::tuple<T, Ts...> &) {
  return "";
}

template <size_t N, typename T, size_t S>
const char *at(const std::array<T, S> &) {
  return "";
}

template <size_t N, typename S, typename T, typename... Ts>
std::string at(const structure<S, T, Ts...> &s) {
  auto m_p = std::get<N>(s.members);
  return '@' + std::to_string(*reinterpret_cast<std::uintptr_t *>(&m_p));
}

template <size_t N, typename T, typename U>
const char *name(const std::pair<T, U> &) {
  constexpr const char *pair[] = {"first", "second"};
  return pair[N];
}

template <size_t N, typename T, typename... Ts>
std::string name(const std::tuple<T, Ts...> &) {
  return std::to_string(N);
}

template <size_t N, typename T, size_t S>
std::string name(const std::array<T, S> &) {
  return std::to_string(N);
}

template <size_t N, typename S, typename T, typename... Ts>
const char *name(const structure<S, T, Ts...> &s) {
  return s.names[N + 1];
}

template <size_t I, typename T>
using element_t = std::remove_cv_t<
    std::remove_reference_t<decltype(get<I>(std::declval<T>()))>>;

#define DECLTYPES(r, data, i, elem) BOOST_PP_COMMA_IF(i) decltype(data::elem)
#define USING_STRUCTURE(s, m)                                                  \
  n2w::structure<s, BOOST_PP_SEQ_FOR_EACH_I(DECLTYPES, s, m)>
#define POINTER_TO_MEMBER(r, data, i, elem) BOOST_PP_COMMA_IF(i) & data::elem
#define MAKE_MEMBER_TUPLE(s, m)                                                \
  std::make_tuple(BOOST_PP_SEQ_FOR_EACH_I(POINTER_TO_MEMBER, s, m))
#define MEM_NAME(r, data, i, elem) BOOST_PP_COMMA_IF(i) BOOST_PP_STRINGIZE(elem)
#define MEMBER_NAMES(m) BOOST_PP_SEQ_FOR_EACH_I(MEM_NAME, _, m)
#define SPECIALIZE_STRUCTURE(s, m)                                             \
  template <>                                                                  \
  const decltype(MAKE_MEMBER_TUPLE(s, m)) USING_STRUCTURE(s, m)::members =     \
      MAKE_MEMBER_TUPLE(s, m);                                                 \
  template <>                                                                  \
  const char *USING_STRUCTURE(s, m)::names[] = {#s, MEMBER_NAMES(m)};
#define CONSTRUCTOR(s, m, o)                                                   \
  USING_STRUCTURE(s, m) o##_v { &o }
}

#endif