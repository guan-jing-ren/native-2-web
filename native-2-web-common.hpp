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

template <unsigned I, typename T> constexpr T mask() {
  return static_cast<T>(static_cast<std::uint8_t>(-1)) << (I * 8);
}

template <unsigned I, typename T> constexpr T mask_byte(T t) {
  return t & mask<I, T>();
}

template <unsigned I, typename T> constexpr T swap_byte(T t) {
  constexpr auto J = sizeof(T) - I - 1;
  constexpr auto S = (J - I) * 8;
  return (mask_byte<I>(t) << S) | (mask_byte<J>(t) >> S);
}

template <typename T, std::size_t... Is>
constexpr T reverse_endian(T t, std::index_sequence<Is...>) {
  const T swapped[] = {swap_byte<Is>(t)...};
  t = 0;
  for (auto s : swapped)
    t |= s;
  return t;
}

template <typename T> constexpr T reverse_endian(T t) {
  return reverse_endian(t, std::make_index_sequence<sizeof(T) / 2>());
}

template <typename T> constexpr auto serial_size = sizeof(T);
// template <> constexpr auto serial_size<void> = sizeof("void");
// template <> constexpr auto serial_size<bool> = sizeof(std::uint8_t);
// template <> constexpr auto serial_size<wchar_t> = sizeof(char32_t);
constexpr size_t P = serial_size<double>;

template <std::size_t P, typename T>
constexpr auto calc_padding(std::size_t count = 1) {
  return P == 0 ? 0 : (P - ((count * serial_size<T>) % P)) % P;
}

#endif