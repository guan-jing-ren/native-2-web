#ifndef _NATIVE_2_WEB_SERIALIZE_HPP_
#define _NATIVE_2_WEB_SERIALIZE_HPP_

namespace n2w {
using namespace std;

constexpr size_t P = serial_size<double>;

template <typename T, typename I> T serialize_number(I &i) {
  static_assert(is_arithmetic<T>::value, "Not an arithmetic type");
  static_assert(is_same<uint8_t, remove_reference_t<decltype(*i)>>::value,
                "Not dereferenceable or uint8_t iterator");

  T t = 0;
  copy_n(i, serial_size<T>, reinterpret_cast<uint8_t *>(&t));
  i += serial_size<T> + calc_padding<P, T>();
  return t;
}

template <typename T, typename I, typename J>
void serialize_numbers(uint32_t count, I &i, J j) {
  static_assert(is_arithmetic<T>::value, "Not an arithmetic type");
  static_assert(is_same<uint8_t, remove_reference_t<decltype(*i)>>::value,
                "Not dereferenceable or uint8_t iterator");
  // static_assert(is_same<T, remove_reference_t<decltype(*j)>>::value,
  // "Output iterator does not dereference T");

  generate_n(j, count, [&i]() { return serialize_number<T>(i); });
  i += calc_padding<P, T>(count);
}

template <typename T, typename I, typename J>
void serialize_objects(uint32_t count, I &i, J j);

template <typename T, typename I, typename J>
void serialize_sequence(uint32_t count, I &i, J j, true_type) {
  serialize_numbers<T>(count, i, j);
}

template <typename T, typename I, typename J>
void serialize_sequence(uint32_t count, I &i, J j, false_type) {
  serialize_objects<T>(count, i, j);
}

template <typename T, typename I, typename J>
void serialize_sequence(I &i, J j) {
  auto count = serialize_number<uint32_t>(i);
  serialize_sequence<T>(count, i, j, is_arithmetic<T>{});
}

template <typename T, typename U, typename I, typename A>
void serialize_associative(I &i, const A &a) {
  auto count = serialize_number<uint32_t>(i);
  vector<T> v_t;
  v_t.reserve(count);
  vector<U> v_u;
  v_u.reserve(count);
  serialize_sequence<T>(count, i, back_inserter(v_t), is_arithmetic<T>{});
  serialize_sequence<U>(count, i, back_inserter(v_u), is_arithmetic<U>{});
  a = inner_product(begin(v_t), end(v_t), begin(v_u), a,
                    [](A &a, pair<T, U> p) {
                      a.emplace(p);
                      return a;
                    },
                    [](T &t, U &u) { return make_pair(t, u); });
}

template <typename I, typename T, size_t... Is>
void serialize_heterogenous(I &i, const T &t, std::index_sequence<Is...>);

template <typename I, typename T>
auto serialize(I &i, const T &t) -> enable_if_t<is_arithmetic<T>::value> {
  t = serialize_number<T>(i);
}

template <typename I, typename T, size_t N>
void serialize(I &i, const T (&t)[N]) {
  serialize_sequence<T>(N, i, t, is_arithmetic<T>{});
}
template <typename I, typename T, size_t N>
void serialize(I &i, const array<T, N> &t) {
  serialize_sequence<T>(N, i, begin(t), is_arithmetic<T>{});
}
template <typename I, typename T, typename U>
void serialize(I &i, const pair<T, U> &t) {
  serialize_heterogenous(i, t, std::make_index_sequence<2>{});
}
template <typename I, typename T, typename... Ts>
void serialize(I &i, const tuple<T, Ts...> &t) {
  serialize_heterogenous(i, t, std::make_index_sequence<sizeof...(Ts) + 1>{});
}
template <typename I, typename T, typename... Traits>
void serialize(I &i, const basic_string<T, Traits...> &t) {
  serialize_sequence<T>(i, back_inserter(t));
}
template <typename I, typename T, typename... Traits>
void serialize(I &i, const vector<T, Traits...> &t) {
  serialize_sequence<T>(i, back_inserter(t));
}
template <typename I, typename T, typename... Traits>
void serialize(I &i, const list<T, Traits...> &t) {
  serialize_sequence<T>(i, back_inserter(t));
}
template <typename I, typename T, typename... Traits>
void serialize(I &i, const forward_list<T, Traits...> &t) {
  serialize_sequence<T>(i, back_inserter(t));
}
template <typename I, typename T, typename... Traits>
void serialize(I &i, const deque<T, Traits...> &t) {
  serialize_sequence<T>(i, back_inserter(t));
}
template <typename I, typename T, typename... Traits>
void serialize(I &i, const set<T, Traits...> &t) {
  serialize_sequence<T>(i, inserter(t, begin(t)));
}
template <typename I, typename T, typename U, typename... Traits>
void serialize(I &i, const map<T, U, Traits...> &t) {
  serialize_associative<T, U>(i, t);
}
template <typename I, typename T, typename... Traits>
void serialize(I &i, const unordered_set<T, Traits...> &t) {
  serialize_sequence<T>(i, inserter(t, begin(t)));
}
template <typename I, typename T, typename U, typename... Traits>
void serialize(I &i, const unordered_map<T, U, Traits...> &t) {
  serialize_associative<T, U>(i, t);
}
template <typename I, typename T, typename... Traits>
void serialize(I &i, const multiset<T, Traits...> &t) {
  serialize_sequence<T>(i, inserter(t, begin(t)));
}
template <typename I, typename T, typename U, typename... Traits>
void serialize(I &i, const multimap<T, U, Traits...> &t) {
  serialize_associative<T, U>(i, t);
}
template <typename I, typename T, typename... Traits>
void serialize(I &i, const unordered_multiset<T, Traits...> &t) {
  serialize_sequence<T>(i, inserter(t, begin(t)));
}
template <typename I, typename T, typename U, typename... Traits>
void serialize(I &i, const unordered_multimap<T, U, Traits...> &t) {
  serialize_associative<T, U>(i, t);
}
template <typename I, typename... Ts>
void serialize(I &i, const structure<Ts...> &t) {
  serialize_heterogenous(i, t, std::make_index_sequence<sizeof...(Ts)>{});
}

template <typename T, typename I, typename J>
void serialize_objects(uint32_t count, I &i, J j) {
  generate_n(j, count, [&i]() {
    T t;
    serialize(i, t);
    return t;
  });
}

template <typename T, typename I> int serialize_index(I &i, const T &t) {
  serialize(i, t);
  return 0;
}

template <typename I, typename T, size_t... Is>
void serialize_heterogenous(I &i, const T &t, std::index_sequence<Is...>) {
  using std::get;
  for (auto rc : {serialize_index(i, get<Is>(t))...})
    (void)rc;
}
}

#endif