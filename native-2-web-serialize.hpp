#ifndef _NATIVE_2_WEB_SERIALIZE_HPP_
#define _NATIVE_2_WEB_SERIALIZE_HPP_

namespace n2w {
using namespace std;

constexpr size_t P = serial_size<double>;

template <typename T, typename I> T serialize_number(T t, I &i) {
  static_assert(is_arithmetic<T>::value, "Not an arithmetic type");
  static_assert(is_same<uint8_t, remove_reference_t<decltype(*i)>>::value,
                "Not dereferenceable or uint8_t iterator");

  T t = 0;
  copy_n(i, serial_size<T>, reinterpret_cast<uint8_t *>(&t));
  i += serial_size<T> + calc_padding<P, T>();
  return t;
}

template <typename T, typename I, typename J>
void serialize_numbers(uint32_t count, J j, I &i) {
  static_assert(is_arithmetic<T>::value, "Not an arithmetic type");
  static_assert(is_same<uint8_t, remove_reference_t<decltype(*i)>>::value,
                "Not dereferenceable or uint8_t iterator");
  // static_assert(is_same<T, remove_reference_t<decltype(*j)>>::value,
  // "Output iterator does not dereference T");

  generate_n(j, count, [&i]() { return serialize_number<T>(i); });
  i += calc_padding<P, T>(count);
}

template <typename T, typename I, typename J>
void serialize_objects(uint32_t count, J j, I &i);

template <typename T, uint32_t N, typename I, typename J>
void serialize_sequence(J j, I &i, true_type) {
  serialize_numbers<T>(N, j, i);
}

template <typename T, uint32_t N, typename I, typename J>
void serialize_sequence(J j, I &i, false_type) {
  serialize_objects<T>(N, j, i);
}

template <typename T, uint32_t N, typename I, typename J>
void serialize_sequence(J j, I &i) {
  serialize_sequence<T, N>(j, i, is_arithmetic<T>{});
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
  auto count = serialize_number<uint32_t>(i);
  vector<T> v_t;
  v_t.reserve(count);
  vector<U> v_u;
  v_u.reserve(count);
  serialize_sequence<T>(count, i, begin(v_t), is_arithmetic<T>{});
  serialize_sequence<U>(count, i, begin(v_u), is_arithmetic<U>{});
  a = inner_product(begin(v_t), end(v_t), begin(v_u), a,
                    [](A &a, pair<T, U> p) {
                      a.emplace(p);
                      return a;
                    },
                    [](T &t, U &u) { return make_pair(t, u); });
}

template <typename I, typename T, size_t... Is>
void serialize_heterogenous(const T &t, std::index_sequence<Is...>, I &i);

template <typename I, typename T>
auto serialize(const T &t, I &i) -> enable_if_t<is_arithmetic<T>::value> {
  serialize_number(t, i);
}

template <typename I, typename T, size_t N>
void serialize(const T (&t)[N], I &i) {
  serialize_sequence<T>(N, t, i, is_arithmetic<T>{});
}
template <typename I, typename T, size_t N>
void serialize(const array<T, N> &t, I &i) {
  serialize_sequence<T>(N, cbegin(t), i, is_arithmetic<T>{});
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
  serialize_sequence<T>(cbegin(t), i);
}
template <typename I, typename T, typename... Traits>
void serialize(const vector<T, Traits...> &t, I &i) {
  serialize_sequence<T>(cbegin(t), i);
}
template <typename I, typename T, typename... Traits>
void serialize(const list<T, Traits...> &t, I &i) {
  serialize_sequence<T>(cbegin(t), i);
}
template <typename I, typename T, typename... Traits>
void serialize(const forward_list<T, Traits...> &t, I &i) {
  serialize_sequence<T>(cbegin(t), i);
}
template <typename I, typename T, typename... Traits>
void serialize(const deque<T, Traits...> &t, I &i) {
  serialize_sequence<T>(cbegin(t), i);
}
template <typename I, typename T, typename... Traitbegins>
void serialize(const set<T, Traits...> &t, I &i) {
  serialize_sequence<T>(cbegin(t), i));
}
template <typename I, typename T, typename U, typename... Traits>
void serialize(const map<T, U, Traits...> &t, I &i) {
  serialize_associative<T, U>(t, i);
}
template <typename I, typename T, typename... Traits>
void serialize(const unordered_set<T, Traits...> &t, I &i) {
  serialize_sequence<T>(cbegin(t), i));
}
template <typename I, typename T, typename U, typename... Traits>
void serialize(const unordered_map<T, U, Traits...> &t, I &i) {
  serialize_associative<T, U>(t, i);
}
template <typename I, typename T, typename... Traits>
void serialize(const multiset<T, Traits...> &t, I &i) {
  serialize_sequence<T>(cbegin(t), i));
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
template <typename I, typename... Ts>
void serialize(const structure<Ts...> &t, I &i) {
  serialize_heterogenous(t, std::make_index_sequence<sizeof...(Ts)>{}, i);
}

template <typename T, typename I, typename J>
void serialize_objects(uint32_t count, I &i, J j) {
  generate_n(j, count, [&i]() {
    T t;
    serialize(t, i);
    return t;
  });
}

template <typename T, typename I> int serialize_index(const T &t, I &i) {
  serialize(t, i);
  return 0;
}

template <typename I, typename T, size_t... Is>
void serialize_heterogenous(const T &t, std::index_sequence<Is...>, I &i) {
  using std::get;
  for (auto rc : {serialize_index(get<Is>(t), i)...})
    (void)rc;
}
}

#endif