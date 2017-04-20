#ifndef _NATIVE_2_WEB_READWRITE_HPP_
#define _NATIVE_2_WEB_READWRITE_HPP_

#include "native-2-web-deserialize.hpp"
#include "native-2-web-serialize.hpp"
#include <locale>

#define READ_WRITE_SPEC(s, m, ...)                                             \
  MANGLE_SPEC(s, m, __VA_ARGS__);                                              \
  SPECIALIZE_STRUCTURE(s, m, __VA_ARGS__);                                     \
  SERIALIZE_SPEC(s, m, __VA_ARGS__);                                           \
  DESERIALIZE_SPEC(s, m, __VA_ARGS__);                                         \
  EQUALITY_SPEC(s, m, __VA_ARGS__);                                            \
  DEBUG_SPEC(s, m, __VA_ARGS__);

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
  return f(forward<tuple_element_t<N, decltype(args)>>(get<N>(args))...);
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

template <typename C> constexpr char indent_char = 0;
template <> constexpr char indent_char<char> = ' ';
template <> constexpr wchar_t indent_char<wchar_t> = L' ';
template <> constexpr char16_t indent_char<char16_t> = u' ';
template <> constexpr char32_t indent_char<char32_t> = U' ';

constexpr size_t tab_size = 2;
template <typename C, size_t I = 0>
const basic_string<C> indent = basic_string<C>(I *tab_size, indent_char<C>);

template <typename T> struct printer;
template <typename O, typename T> O &debug_print(O &o, const T &t) {
  return printer<T>::template debug_print(o, t);
}
template <typename O, size_t N> O &debug_print(O &o, const char (&t)[N]) {
  return o << mangled<basic_string<char>>() << ":=\"" << t << '"';
}

template <> struct printer<bool> {
  template <size_t I = 0, typename O> static O &debug_print(O &o, bool t) {
    return o << indent<typename O::char_type, I> << mangled<bool>() << ":='"
             << t << "'";
  }
};

template <size_t I = 0, typename O, typename T>
O &print_sequence(O &o, const T &t, size_t count) {
  using T2 = remove_cv_t<remove_reference_t<decltype(*begin(t))>>;
  o << indent<typename O::char_type,
              I> << mangled<remove_cv_t<remove_reference_t<T>>>()
    << ":=[" << (is_arithmetic<T2>::value || !count ? "" : "\n");
  size_t i = 0;
  if (count)
    for (auto &_t : t) {
      printer<T2>::template debug_print<is_arithmetic<T2>::value ? 0u : I + 1>(
          o, _t);
      if (++i < count)
        o << ", " << (is_arithmetic<T2>::value ? "" : "\n");
    }
  o << (is_arithmetic<T2>::value || !count
            ? ""
            : "\n" + indent<typename O::char_type, I>)
    << ']';
  return o;
}

template <typename T, size_t... Is>
constexpr bool is_all_num(index_sequence<Is...>) {
  bool is_num = true;
  for (auto b : {is_arithmetic<
           remove_cv_t<remove_reference_t<element_t<Is, T>>>>::value...})
    is_num &= b;
  return is_num;
}

template <size_t I = 0, typename O, typename T, size_t... Is>
O &print_heterogenous(O &o, T &t, index_sequence<Is...>) {
  constexpr bool is_num = is_all_num<T>(index_sequence<Is...>{});
  o << indent<typename O::char_type,
              I> << mangled<remove_cv_t<remove_reference_t<T>>>()
    << ":={" << (is_num ? "" : "\n");
  for (auto &_o :
       {&(printer<remove_cv_t<remove_reference_t<element_t<Is, T>>>>::
              template debug_print<(is_num ? 0u : (I + 1))>(o, get<Is>(t))
          << "\t`" << name<Is>(t) << '`' << at<Is>(t)
          << ((Is + 1) < sizeof...(Is) ? string{", "} + (is_num ? "" : "\n")
                                       : ""))...})
    (void)_o;
  o << (is_num ? "" : "\n" + indent<typename O::char_type, I>) << '}';
  return o;
}

template <typename T> struct printer {
  template <size_t I = 0, typename O>
  static auto debug_print(O &o, T t)
      -> enable_if_t<is_arithmetic<T>::value, O &> {
    return o << indent<typename O::char_type,
                       I> << mangled<remove_cv_t<remove_reference_t<T>>>()
             << ":='" << t << "'";
  }
  template <size_t I = 0, typename O>
  static auto debug_print(O &o, T t) -> enable_if_t<is_enum<T>::value, O &> {
    return o << indent<typename O::char_type, I> << enumeration<T>::type_name
             << ":='" << enumeration<T>::e_to_str[t] << "'";
  }
};
template <typename T, size_t N> struct printer<T[N]> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const T (&t)[N]) {
    return print_sequence<I>(o, t, N);
  }
};
template <typename T, size_t N> struct printer<array<T, N>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const array<T, N> &t) {
    return print_sequence<I>(o, t, N);
  }
};
template <typename T, typename U> struct printer<pair<T, U>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const pair<T, U> &t) {
    return print_heterogenous<I>(o, t, make_index_sequence<2>{});
  }
};
template <typename T, typename... Ts> struct printer<tuple<T, Ts...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const tuple<T, Ts...> &t) {
    return print_heterogenous<I>(o, t,
                                 make_index_sequence<sizeof...(Ts) + 1>{});
  }
};
template <size_t N> struct printer<char[N]> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const char (&t)[N]) {
    return o << mangled<basic_string<char>>() << ":=\"" << t << '"';
  }
};
template <typename T, typename... Traits>
struct printer<basic_string<T, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const basic_string<T, Traits...> &t) {
    struct cvt : codecvt<T, typename O::char_type, mbstate_t> {};
    wstring_convert<cvt, T> cvter{
        "Could not convert from " + mangle<basic_string<T, Traits...>> +
        " to " + mangle<basic_string<typename O::char_type, Traits...>>};
    return o << indent<typename O::char_type,
                       I> << mangled<basic_string<T, Traits...>>()
             << ":=\"" << cvter.to_bytes(t) << '"';
  }
};
template <typename T, typename... Traits> struct printer<vector<T, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const vector<T, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename... Traits> struct printer<list<T, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const list<T, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename... Traits>
struct printer<forward_list<T, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const forward_list<T, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename... Traits> struct printer<deque<T, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const deque<T, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename... Traits> struct printer<set<T, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const set<T, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename U, typename... Traits>
struct printer<map<T, U, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const map<T, U, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename... Traits>
struct printer<unordered_set<T, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const unordered_set<T, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename U, typename... Traits>
struct printer<unordered_map<T, U, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const unordered_map<T, U, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename... Traits>
struct printer<multiset<T, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const multiset<T, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename U, typename... Traits>
struct printer<multimap<T, U, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const multimap<T, U, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename... Traits>
struct printer<unordered_multiset<T, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const unordered_multiset<T, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename T, typename U, typename... Traits>
struct printer<unordered_multimap<T, U, Traits...>> {
  template <size_t I = 0, typename O>
  static O &debug_print(O &o, const unordered_multimap<T, U, Traits...> &t) {
    return print_sequence<I>(o, t, t.size());
  }
};
template <typename S, typename T, typename... Ts, typename... Bs>
struct printer<structure<S, tuple<T, Ts...>, tuple<Bs...>>> {
  template <size_t I = 0, typename O, typename B>
  static int debug_print(O &o, const B &b) {
    printer<B>::template debug_print<I + 1>(o, b);
    return 0;
  }
  template <size_t I = 0, typename O> static int debug_print(O &o) { return 0; }
  template <size_t I = 0, typename O>
  static O &debug_print(O &o,
                        const structure<S, tuple<T, Ts...>, tuple<Bs...>> &t) {
    initializer_list<int> rc = {
        debug_print<I>(o, *static_cast<const Bs *>(t.s_read))...};
    (void)rc;
    return print_heterogenous<I>(o, t, make_index_sequence<sizeof...(Ts) + 1>{})
           << "\t`" << *begin(t.names) << '`';
  }
};

template <typename T> struct assignable_filler { using type = remove_cv_t<T>; };

template <typename T, typename U> struct assignable_filler<pair<T, U>> {
  using type = pair<remove_cv_t<T>, remove_cv_t<U>>;
};

template <typename T, typename... Ts>
struct assignable_filler<tuple<T, Ts...>> {
  using type = tuple<remove_cv_t<T>, remove_cv_t<Ts>...>;
};

template <typename T>
using assignable_filler_t = typename assignable_filler<T>::type;

template <typename T, size_t V = 5> struct filler {
  size_t iteration = 0;
  assignable_filler_t<T> t;
  vector<assignable_filler_t<T>> alphabet;

  void sort_unique() {
    sort(begin(alphabet), end(alphabet));
    alphabet.erase(unique(begin(alphabet), end(alphabet)), end(alphabet));
  }

  template <typename U = bool>
  auto construct(bool &b) -> enable_if_t<is_same<U, bool>{}> {
    alphabet.insert(end(alphabet),
                    {false, true, 0, 1, static_cast<bool>(-1ll),
                     numeric_limits<bool>::min(), numeric_limits<bool>::max()});
  }
  template <typename U = char>
  auto construct(char &u) -> enable_if_t<is_same<U, char>{}> {
    const char sample[] = "丑乗 09azAZ<&~^[({\\@#";
    alphabet.insert(end(alphabet), begin(sample), end(sample));
  }
  template <typename U = wchar_t>
  auto construct(wchar_t &u) -> enable_if_t<is_same<U, wchar_t>{}> {
    const wchar_t sample[] = L"丑乗 09azAZ<&~^[({\\@#";
    alphabet.insert(end(alphabet), begin(sample), end(sample));
  }
  template <typename U = char16_t>
  auto construct(char16_t &u) -> enable_if_t<is_same<U, char16_t>{}> {
    const char16_t sample[] = u"丑乗 09azAZ<&~^[({\\@#";
    alphabet.insert(end(alphabet), begin(sample), end(sample));
  }
  template <typename U = char32_t>
  auto construct(char32_t &u) -> enable_if_t<is_same<U, char32_t>{}> {
    const char32_t sample[] = U"丑乗 09azAZ<&~^[({\\@#";
    alphabet.insert(end(alphabet), begin(sample), end(sample));
  }
  template <typename U> auto construct(U &u) -> enable_if_t<is_integral<U>{}> {
    alphabet.insert(end(alphabet),
                    {0, 1, static_cast<U>(-1ll), numeric_limits<U>::min(),
                     numeric_limits<U>::max()});
    sort_unique();
  }
  template <typename U>
  auto construct(U &u) -> enable_if_t<is_floating_point<U>{}> {
    alphabet.insert(end(alphabet),
                    {0, 1, static_cast<U>(-1ll), numeric_limits<U>::min(),
                     numeric_limits<U>::lowest(), numeric_limits<U>::max(),
                     numeric_limits<U>::denorm_min()});
    sort_unique();
  }

  template <typename T2, typename U>
  static void sequence_init(size_t count, U u) {
    filler<remove_cv_t<remove_reference_t<T2>>, V> fill;
    generate_n(u, count, fill);
  }

  template <typename U>
  auto construct(U &u) -> enable_if_t<!is_void<typename U::iterator>{}> {
    sequence_init<typename U::value_type>(V, inserter(u, end(u)));
  }
  template <typename U, size_t C> void construct(U (&u)[C]) {
    sequence_init<U>(C, begin(u));
  }
  template <typename U, size_t C> void construct(array<U, C> &u) {
    sequence_init<U>(C, begin(u));
  }

  template <typename U> void construct_combine(U &u) { alphabet.push_back(u); }

  template <typename U, size_t I, size_t... Is>
  void construct_combine(U &u, integral_constant<size_t, I>,
                         integral_constant<size_t, Is>...) {
    vector<U> us;
    filler<tuple_element_t<I, U>, V> fill;
    generate_n(back_inserter(us), V, [&u, &fill]() {
      get<I>(u) = fill();
      return u;
    });
    for (auto &&_u : us)
      construct_combine(_u, integral_constant<size_t, Is>{}...);
  }

  template <typename U, size_t... Is>
  void construct(U &u, index_sequence<Is...>) {
    construct_combine(u, integral_constant<size_t, Is>{}...);
  }

  template <typename U, typename U2> void construct(pair<U, U2> &u) {
    construct(t, make_index_sequence<2>{});
  }

  template <typename U, typename... Us> void construct(tuple<U, Us...> &u) {
    construct(t, make_index_sequence<sizeof...(Us) + 1>{});
  }

  filler() {
    construct(t);
    if (!alphabet.empty())
      t = alphabet[iteration++ % alphabet.size()];
  }

  template <typename U = T>
  auto next(T &t) -> enable_if_t<is_arithmetic<U>{}, T> {
    auto old = t;
    t = alphabet[iteration++ % alphabet.size()];
    return old;
  }

  template <typename I, typename U, typename U2>
  bool permute(I first, I last, const pair<const U, U2> &&) {
    return true;
  }

  template <typename I, typename U> bool permute(I first, I last, const U &&) {
    return next_permutation(first, last);
  }

  template <typename U = T> auto next(U &u, bidirectional_iterator_tag) {
    auto old = t;
    permute(begin(u), end(u), typename U::value_type{});
    return old;
  }

  template <typename U = T> auto next(U &u, input_iterator_tag) { return t; }

  template <typename U = T>
  auto next(U &u) -> enable_if_t<!is_void<typename U::iterator>{}, U> {
    return next(
        u, typename iterator_traits<typename U::iterator>::iterator_category{});
  }

  template <typename U, typename U2> auto next(pair<U, U2> &) {
    auto old = t;
    t = alphabet[iteration++ % alphabet.size()];
    return old;
  }
  template <typename U, typename... Us> auto next(tuple<U, Us...> &) {
    auto old = t;
    t = alphabet[iteration++ % alphabet.size()];
    return old;
  }

  T operator()() { return next(t); }
};

#define EQUALITY_SPEC(s, m, ...)                                               \
  bool operator==(const s &l, const s &r) {                                    \
    return USING_STRUCTURE(s, m, __VA_ARGS__){&l} ==                           \
           USING_STRUCTURE(s, m, __VA_ARGS__){&r};                             \
  }                                                                            \
  bool operator!=(const s &l, const s &r) { return !(l == r); }                \
  bool operator<(const s &l, const s &r) {                                     \
    return USING_STRUCTURE(s, m, __VA_ARGS__){&l} <                            \
           USING_STRUCTURE(s, m, __VA_ARGS__){&r};                             \
  }                                                                            \
  bool operator>(const s &l, const s &r) {                                     \
    return USING_STRUCTURE(s, m, __VA_ARGS__){&l} >                            \
           USING_STRUCTURE(s, m, __VA_ARGS__){&r};                             \
  }                                                                            \
  bool operator<=(const s &l, const s &r) {                                    \
    return USING_STRUCTURE(s, m, __VA_ARGS__){&l} <=                           \
           USING_STRUCTURE(s, m, __VA_ARGS__){&r};                             \
  }                                                                            \
  bool operator>=(const s &l, const s &r) {                                    \
    return USING_STRUCTURE(s, m, __VA_ARGS__){&l} >=                           \
           USING_STRUCTURE(s, m, __VA_ARGS__){&r};                             \
  }

#define DEBUG_SPEC(s, m, ...)                                                  \
  namespace n2w {                                                              \
  template <> struct printer<s> {                                              \
    template <size_t I = 0, typename O>                                        \
    static O &debug_print(O &o, const s &_s) {                                 \
      CONSTRUCTOR(s, m, _s, __VA_ARGS__);                                      \
      return printer<decltype(_s_v)>::template debug_print<I>(o, _s_v);        \
    }                                                                          \
  };                                                                           \
  template <size_t V> struct filler<s, V> {                                    \
    size_t iteration = 0;                                                      \
    assignable_filler_t<s> t;                                                  \
    vector<assignable_filler_t<s>> alphabet;                                   \
                                                                               \
    template <typename... Ts> void sink(Ts &&...) {}                           \
                                                                               \
    template <size_t... Is>                                                    \
    s construct(USING_STRUCTURE(s, m, __VA_ARGS__) & d,                        \
                tuple<BOOST_PP_SEQ_FOR_EACH_I(DECLTYPES, s, m)> &&t,           \
                index_sequence<Is...>) {                                       \
      sink((get<Is>(d) = get<Is>(t))...);                                      \
      return *d.s_write;                                                       \
    }                                                                          \
                                                                               \
    filler() {                                                                 \
      filler<tuple<BOOST_PP_SEQ_FOR_EACH_I(DECLTYPES, s, m)>, V> fill;         \
      generate_n(back_inserter(alphabet), V, [this, &fill]() {                 \
        CONSTRUCTOR(s, m, t, __VA_ARGS__);                                     \
        return construct(                                                      \
            t_v, fill(),                                                       \
            make_index_sequence<tuple_size<decltype(fill.t)>{}>{});            \
      });                                                                      \
      if (V)                                                                   \
        t = alphabet[0];                                                       \
    }                                                                          \
                                                                               \
    s operator()() {                                                           \
      auto old = t;                                                            \
      t = alphabet[iteration++ % alphabet.size()];                             \
      return old;                                                              \
    }                                                                          \
  };                                                                           \
  }
}

#endif