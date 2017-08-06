#ifndef _NATIVE_2_WEB_COMMON_HPP_
#define _NATIVE_2_WEB_COMMON_HPP_

#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <boost/preprocessor.hpp>
#include <chrono>
#include <complex>
#include <cstdint>
#include <deque>
#include <experimental/filesystem>
#include <forward_list>
#include <list>
#include <map>
#include <numeric>
#include <optional>
#include <ratio>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace std {
template <typename T> bool less_unordered(const T &l, const T &r) {
  vector<add_pointer_t<add_const_t<remove_reference_t<decltype(*begin(l))>>>>
      l_v;
  auto r_v = l_v;
  transform(cbegin(l), cend(l), back_inserter(l_v),
            [](const auto &l) { return &l; });
  transform(cbegin(r), cend(r), back_inserter(r_v),
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

namespace n2w {

// clang-format off
template <typename T> constexpr bool is_sequence = false;
template <typename T, size_t N> constexpr bool is_sequence<T[N]> = true;
template <typename T, size_t N> constexpr bool is_sequence<std::array<T, N>> = true;
template <typename... Ts> constexpr bool is_sequence<std::vector<Ts...>> = true;
template <typename... Ts> constexpr bool is_sequence<std::list<Ts...>> = true;
template <typename... Ts> constexpr bool is_sequence<std::forward_list<Ts...>> = true;
template <typename... Ts> constexpr bool is_sequence<std::deque<Ts...>> = true;
template <typename... Ts> constexpr bool is_sequence<std::set<Ts...>> = true;
template <typename... Ts> constexpr bool is_sequence<std::unordered_set<Ts...>> = true;
template <typename... Ts> constexpr bool is_sequence<std::multiset<Ts...>> = true;
template <typename... Ts> constexpr bool is_sequence<std::unordered_multiset<Ts...>> = true;

template <typename T> constexpr bool is_pushback_sequence = is_sequence<T>;
template <typename... Ts> constexpr bool is_pushback_sequence<std::set<Ts...>> = false;
template <typename... Ts> constexpr bool is_pushback_sequence<std::unordered_set<Ts...>> = false;
template <typename... Ts> constexpr bool is_pushback_sequence<std::multiset<Ts...>> = false;
template <typename... Ts> constexpr bool is_pushback_sequence<std::unordered_multiset<Ts...>> = false;

template <typename> constexpr bool is_associative = false;
template <typename... Ts> constexpr bool is_associative<std::map<Ts...>> = true;
template <typename... Ts> constexpr bool is_associative<std::unordered_map<Ts...>> = true;
template <typename... Ts> constexpr bool is_associative<std::multimap<Ts...>> = true;
template <typename... Ts> constexpr bool is_associative<std::unordered_multimap<Ts...>> = true;

template <typename> constexpr bool is_heterogenous = false;
template <typename T, typename U> constexpr bool is_heterogenous<std::pair<T, U>> = true;
template <typename... Ts> constexpr bool is_heterogenous<std::tuple<Ts...>> = true;
// clang-format on

template <typename S, typename M, typename B> struct structure;

template <typename S, typename T, typename... Ts, typename... Bs>
struct structure<S, std::tuple<T, Ts...>, std::tuple<Bs...>> {
  const volatile union {
    S *const volatile s_write;
    const S *const volatile s_read;
  };
  static const std::tuple<T S::*, Ts S::*...> members;
  static const std::vector<std::string> names;
  static const std::vector<std::string> base_names;
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

template <size_t N, typename S, typename T, typename... Ts, typename... Bs>
auto &get(const structure<S, std::tuple<T, Ts...>, std::tuple<Bs...>> &s) {
  return s.s_read->*get<N>(s.members);
}

template <size_t N, typename S, typename T, typename... Ts, typename... Bs>
auto &get(structure<S, std::tuple<T, Ts...>, std::tuple<Bs...>> &s) {
  return s.s_write->*get<N>(s.members);
}

template <typename S, typename T, typename... Ts, typename... Bs, size_t... Is>
bool memberwise_equality(
    const structure<S, std::tuple<T, Ts...>, std::tuple<Bs...>> &l,
    const structure<S, std::tuple<T, Ts...>, std::tuple<Bs...>> &r,
    std::index_sequence<Is...>) {
  if (l.s_read == r.s_read)
    return true;

  bool equal = true;
  for (auto rc : std::initializer_list<bool>{
           (equal = equal &&
                    *static_cast<const Bs *>(l.s_read) ==
                        *static_cast<const Bs *>(r.s_read))...})
    if (!equal)
      return equal;
  return equal;

  for (auto rc : {(equal = equal && get<Is>(l) == get<Is>(r))...})
    if (!equal)
      return equal;
  return equal;
}
template <typename S, typename T, typename... Ts, typename... Bs>
bool operator==(
    const structure<S, std::tuple<T, Ts...>, std::tuple<Bs...>> &l,
    const structure<S, std::tuple<T, Ts...>, std::tuple<Bs...>> &r) {
  return memberwise_equality(l, r,
                             std::make_index_sequence<sizeof...(Ts) + 1>{});
}
template <typename S, typename T, typename... Ts, typename... Bs>
bool operator!=(
    const structure<S, std::tuple<T, Ts...>, std::tuple<Bs...>> &l,
    const structure<S, std::tuple<T, Ts...>, std::tuple<Bs...>> &r) {
  return !(l == r);
}

template <typename S, typename T, typename... Ts, typename... Bs, size_t... Is>
bool memberwise_less(
    const structure<S, std::tuple<T, Ts...>, std::tuple<Bs...>> &l,
    const structure<S, std::tuple<T, Ts...>, std::tuple<Bs...>> &r,
    std::index_sequence<Is...>) {
  if (l.s_read == r.s_read)
    return false;

  std::pair<bool, bool> lesser_greater = std::make_pair(false, false);
  for (auto rc : std::initializer_list<decltype(lesser_greater)>{
           (lesser_greater = std::make_pair(
                !lesser_greater.second &&
                    (lesser_greater.first ||
                     *static_cast<const Bs *>(l.s_read) <
                         *static_cast<const Bs *>(r.s_read)),
                !lesser_greater.first &&
                    (lesser_greater.second ||
                     *static_cast<const Bs *>(r.s_read) <
                         *static_cast<const Bs *>(l.s_read))))...}) {
    if (lesser_greater.first)
      return true;
    if (lesser_greater.second)
      return false;
  }

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
template <typename S, typename T, typename... Ts, typename... Bs>
bool operator<(const structure<S, std::tuple<T, Ts...>, std::tuple<Bs...>> &l,
               const structure<S, std::tuple<T, Ts...>, std::tuple<Bs...>> &r) {
  return memberwise_less(l, r, std::make_index_sequence<sizeof...(Ts) + 1>{});
}

template <typename S, typename T, typename... Ts, typename... Bs>
bool operator>(const structure<S, std::tuple<T, Ts...>, std::tuple<Bs...>> &l,
               const structure<S, std::tuple<T, Ts...>, std::tuple<Bs...>> &r) {
  return r < l;
};

template <typename S, typename T, typename... Ts, typename... Bs>
bool operator<=(
    const structure<S, std::tuple<T, Ts...>, std::tuple<Bs...>> &l,
    const structure<S, std::tuple<T, Ts...>, std::tuple<Bs...>> &r) {
  return !(r < l);
};

template <typename S, typename T, typename... Ts, typename... Bs>
bool operator>=(
    const structure<S, std::tuple<T, Ts...>, std::tuple<Bs...>> &l,
    const structure<S, std::tuple<T, Ts...>, std::tuple<Bs...>> &r) {
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

template <size_t N, typename S, typename T, typename... Ts, typename... Bs>
std::string at(const structure<S, std::tuple<T, Ts...>, std::tuple<Bs...>> &s) {
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

template <size_t N, typename S, typename T, typename... Ts, typename... Bs>
std::string
name(const structure<S, std::tuple<T, Ts...>, std::tuple<Bs...>> &s) {
  return s.names[N + 1];
}

template <typename E> struct enumeration {
  static const std::string type_name;
  static std::map<E, std::string> e_to_str;
  static std::unordered_map<std::string, E> str_to_e;
};

template <size_t I, typename T>
using element_t = std::remove_cv_t<
    std::remove_reference_t<decltype(get<I>(std::declval<T>()))>>;

#define N2W__MEMBERS BOOST_PP_VARIADIC_TO_SEQ
#define N2W__BASES(...) __VA_ARGS__
#define N2W__DECLTYPES(r, data, i, elem)                                       \
  BOOST_PP_COMMA_IF(i) decltype(data::elem)
#define N2W__USING_STRUCTURE(s, m, ...)                                        \
  n2w::structure<s, std::tuple<BOOST_PP_SEQ_FOR_EACH_I(N2W__DECLTYPES, s, m)>, \
                 std::tuple<__VA_ARGS__>>
#define N2W__POINTER_TO_MEMBER(r, data, i, elem)                               \
  BOOST_PP_COMMA_IF(i) & data::elem
#define N2W__MAKE_MEMBER_TUPLE(s, m)                                           \
  std::make_tuple(BOOST_PP_SEQ_FOR_EACH_I(N2W__POINTER_TO_MEMBER, s, m))
#define N2W__MEM_NAME(r, data, i, elem)                                        \
  BOOST_PP_COMMA_IF(i) BOOST_PP_STRINGIZE(elem)
#define N2W__MEMBER_NAMES(m) BOOST_PP_SEQ_FOR_EACH_I(N2W__MEM_NAME, _, m)
#define N2W__SPECIALIZE_STRUCTURE(s, m, ...)                                   \
  template <>                                                                  \
  const decltype(N2W__MAKE_MEMBER_TUPLE(s, m)) N2W__USING_STRUCTURE(           \
      s, m, __VA_ARGS__)::members = N2W__MAKE_MEMBER_TUPLE(s, m);              \
  template <>                                                                  \
  const std::vector<std::string> N2W__USING_STRUCTURE(                         \
      s, m, __VA_ARGS__)::names{#s, N2W__MEMBER_NAMES(m)};                     \
  template <>                                                                  \
  const std::vector<std::string> N2W__USING_STRUCTURE(                         \
      s, m, __VA_ARGS__)::base_names{                                          \
      N2W__MEMBER_NAMES(BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))};
#define N2W__CONSTRUCTOR(s, m, o, ...)                                         \
  N2W__USING_STRUCTURE(s, m, __VA_ARGS__) o##_v { &o }

#define N2W__E_S_PAIR(r, data, i, elem)                                        \
  BOOST_PP_COMMA_IF(i) { elem, BOOST_PP_STRINGIZE(elem) }
#define N2W__S_E_PAIR(r, data, i, elem)                                        \
  BOOST_PP_COMMA_IF(i) { BOOST_PP_STRINGIZE(elem), elem }
#define N2W__ENUM_TO_STRING(m) BOOST_PP_SEQ_FOR_EACH_I(N2W__E_S_PAIR, _, m)
#define N2W__STRING_TO_ENUM(m) BOOST_PP_SEQ_FOR_EACH_I(N2W__S_E_PAIR, _, m)
#define N2W__SPECIALIZE_ENUM(e, m)                                             \
  template <> struct n2w::mangle<e> : n2w::mangle<enumeration<e>> {};          \
  template <> const std::string n2w::enumeration<e>::type_name = #e;           \
  template <>                                                                  \
  std::map<e, std::string> n2w::enumeration<e>::e_to_str = {                   \
      N2W__ENUM_TO_STRING(m)};                                                 \
  template <>                                                                  \
  std::unordered_map<std::string, e> n2w::enumeration<e>::str_to_e = {         \
      N2W__STRING_TO_ENUM(m)};
}

#endif