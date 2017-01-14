#ifndef _NATIVE_2_WEB_MANGLESPEC_HPP_
#define _NATIVE_2_WEB_MANGLESPEC_HPP_

#include "native-2-web-common.hpp"

namespace n2w {

using namespace std;

template <typename T, typename... Ts> struct csv { static const string value; };
template <typename T, typename... Ts>
const string csv<T, Ts...>::value = csv<T>::value + ',' + csv<Ts...>::value;
template <typename T> struct csv<T> { static const string value; };

template <typename> struct mangle_prefix {
  static constexpr const char *value = "";
};
template <typename T> using mangle_prefix_v = typename mangle_prefix<T>::value;
template <typename T, size_t N> struct mangle_prefix<T[N]> {
  static constexpr const char *value = "c[";
};
template <typename T, size_t N> struct mangle_prefix<array<T, N>> {
  static constexpr const char *value = "a[";
};
template <typename T, typename U> struct mangle_prefix<pair<T, U>> {
  static constexpr const char value = '<';
};
template <typename T, typename... Ts> struct mangle_prefix<tuple<T, Ts...>> {
  static constexpr const char value = '(';
};
template <typename T, typename... Traits>
struct mangle_prefix<basic_string<T, Traits...>> {
  static constexpr const char value = '"';
};
template <typename T, typename... Traits>
struct mangle_prefix<vector<T, Traits...>> {
  static constexpr const char *value = "v[";
};
template <typename T, typename... Traits>
struct mangle_prefix<list<T, Traits...>> {
  static constexpr const char *value = "l[";
};
template <typename T, typename... Traits>
struct mangle_prefix<forward_list<T, Traits...>> {
  static constexpr const char *value = "g[";
};
template <typename T, typename... Traits>
struct mangle_prefix<deque<T, Traits...>> {
  static constexpr const char *value = "d[";
};
template <typename T, typename... Traits>
struct mangle_prefix<set<T, Traits...>> {
  static constexpr const char *value = "s[";
};
template <typename T, typename U, typename... Traits>
struct mangle_prefix<map<T, U, Traits...>> {
  static constexpr const char *value = "m{";
};
template <typename T, typename... Traits>
struct mangle_prefix<unordered_set<T, Traits...>> {
  static constexpr const char *value = "h[";
};
template <typename T, typename U, typename... Traits>
struct mangle_prefix<unordered_map<T, U, Traits...>> {
  static constexpr const char *value = "h{";
};
template <typename T, typename... Traits>
struct mangle_prefix<multiset<T, Traits...>> {
  static constexpr const char *value = "S[";
};
template <typename T, typename U, typename... Traits>
struct mangle_prefix<multimap<T, U, Traits...>> {
  static constexpr const char *value = "M{";
};
template <typename T, typename... Traits>
struct mangle_prefix<unordered_multiset<T, Traits...>> {
  static constexpr const char *value = "H[";
};
template <typename T, typename U, typename... Traits>
struct mangle_prefix<unordered_multimap<T, U, Traits...>> {
  static constexpr const char *value = "H{";
};

template <typename T> struct mangle {
  static constexpr const char *value = "";
};
template <typename T> using mangle_v = typename mangle<T>::value;

template <> struct mangle<void> { static constexpr const char value = '0'; };
template <> struct mangle<bool> { static constexpr const char value = 'b'; };
template <> struct mangle<wchar_t> {
  static constexpr const char *value = "'w";
};
template <> struct mangle<char16_t> {
  static constexpr const char *value = "'4";
};
template <> struct mangle<char32_t> {
  static constexpr const char *value = "'5";
};
template <> struct mangle<int8_t> {
  static constexpr const char *value = "i3";
};
template <> struct mangle<int16_t> {
  static constexpr const char *value = "i4";
};
template <> struct mangle<int32_t> {
  static constexpr const char *value = "i5";
};
template <> struct mangle<int64_t> {
  static constexpr const char *value = "i6";
};
template <> struct mangle<uint8_t> {
  static constexpr const char *value = "u3";
};
template <> struct mangle<uint16_t> {
  static constexpr const char *value = "u4";
};
template <> struct mangle<uint32_t> {
  static constexpr const char *value = "u5";
};
template <> struct mangle<uint64_t> {
  static constexpr const char *value = "u6";
};
template <> struct mangle<float> { static constexpr const char *value = "f5"; };
template <> struct mangle<double> {
  static constexpr const char *value = "f6";
};
template <> struct mangle<long double> {
  static constexpr const char *value = "f7";
};

template <typename T, size_t N> struct mangle<T[N]> {
  static const string value;
};
template <typename T, size_t N>
const string mangle<T[N]>::value = mangle_prefix<T[N]>::value +
                                   string{mangle<T>::value} + ',' +
                                   to_string(N);
template <typename T, size_t N> struct mangle<array<T, N>> {
  static const string value;
};
template <typename T, size_t N>
const string mangle<array<T, N>>::value = mangle_prefix<array<T, N>>::value +
                                          string{mangle<T>::value} + ',' +
                                          to_string(N);
template <typename T, typename U> struct mangle<pair<T, U>> {
  static const string value;
};
template <typename T, typename U>
const string mangle<pair<T, U>>::value =
    mangle_prefix<pair<T, U>>::value + string{mangle<T>::value} + ',' +
    string{mangle<U>::value};
template <typename T, typename... Ts> struct mangle<tuple<T, Ts...>> {
  static const string value;
};
template <typename T, typename... Ts>
const string mangle<tuple<T, Ts...>>::value =
    mangle_prefix<tuple<T, Ts...>>::value + std::string{csv<T, Ts...>::value} +
    ')';
template <typename T, typename... Traits>
struct mangle<basic_string<T, Traits...>> {
  static const string value;
};
template <typename T, typename... Traits>
const string mangle<basic_string<T, Traits...>>::value =
    mangle_prefix<basic_string<T, Traits...>>::value + string{mangle<T>::value};
template <typename T, typename... Traits> struct mangle<vector<T, Traits...>> {
  static const string value;
};
template <typename T, typename... Traits>
const string mangle<vector<T, Traits...>>::value =
    mangle_prefix<vector<T, Traits...>>::value + string{mangle<T>::value};
template <typename T, typename... Traits> struct mangle<list<T, Traits...>> {
  static const string
      value; // mangle_prefix_v<list<T, Traits...>> + string{mangle_v<T>};
};
template <typename T, typename... Traits>
struct mangle<forward_list<T, Traits...>> {
  static const string value; // mangle_prefix_v<forward_list<T, Traits...>> +
                             // string{mangle_v<T>};
};
template <typename T, typename... Traits> struct mangle<deque<T, Traits...>> {
  static const string
      value; // mangle_prefix_v<deque<T, Traits...>> + string{mangle_v<T>};
};
template <typename T, typename... Traits> struct mangle<set<T, Traits...>> {
  static const string
      value; // mangle_prefix_v<set<T, Traits...>> + string{mangle_v<T>};
};
template <typename T, typename U, typename... Traits>
struct mangle<map<T, U, Traits...>> {
  static const string value; // mangle_prefix_v<map<T, U, Traits...>> +
                             // string{mangle_v<T>} + ':' + string{mangle_v<U>};
};
template <typename T, typename... Traits>
struct mangle<unordered_set<T, Traits...>> {
  static const string value; // mangle_prefix_v<unordered_set<T, Traits...>> +
                             // string{mangle_v<T>};
};
template <typename T, typename U, typename... Traits>
struct mangle<unordered_map<T, U, Traits...>> {
  static const string value; // mangle_prefix_v<unordered_map<T, U, Traits...>>
                             // +
                             // string{mangle_v<T>} + ':' + string{mangle_v<U>};
};
template <typename T, typename... Traits>
struct mangle<multiset<T, Traits...>> {
  static const string
      value; // mangle_prefix_v<multiset<T, Traits...>> + string{mangle_v<T>};
};
template <typename T, typename U, typename... Traits>
struct mangle<multimap<T, U, Traits...>> {
  static const string value; // mangle_prefix_v<multimap<T, U, Traits...>> +
                             // string{mangle_v<T>} + ':' + string{mangle_v<U>};
};
template <typename T, typename... Traits>
struct mangle<unordered_multiset<T, Traits...>> {
  static const string value; // mangle_prefix_v<unordered_multiset<T,
                             // Traits...>> + string{mangle_v<T>};
};
template <typename T, typename U, typename... Traits>
struct mangle<unordered_multimap<T, U, Traits...>> {
  static const string value; // mangle_prefix_v<unordered_multimap<T, U,
                             // Traits...>> +  string{mangle_v<T>} + ':' +
                             // string{mangle_v<U>};
};

template <typename T> const std::string csv<T>::value = mangle<T>::value;
}

#endif