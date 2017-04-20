#ifndef _NATIVE_2_WEB_DESERIALIZE_HPP_
#define _NATIVE_2_WEB_DESERIALIZE_HPP_

#include "native-2-web-common.hpp"
#include "native-2-web-manglespec.hpp"
#include <locale>

namespace n2w {
using namespace std;

template <typename T> struct deserializer;
template <typename I, typename T> I deserialize(I i, T &t) {
  deserializer<T>::deserialize(i, t);
  return i;
}

template <typename T, typename I> T deserialize_number(I &i) {
  static_assert(is_arithmetic<T>::value, "Not an arithmetic type");
  // static_assert(is_same<uint8_t, remove_reference_t<decltype(*i)>>::value,
  // "Not dereferenceable or uint8_t iterator");

  T t = 0;
  copy_n(i, serial_size<T>, reinterpret_cast<uint8_t *>(&t));
  i += serial_size<T>;
  return t;
}

template <typename T, typename I, typename J>
void deserialize_numbers(uint32_t count, I &i, J j) {
  static_assert(is_arithmetic<T>::value, "Not an arithmetic type");
  // static_assert(is_same<uint8_t, remove_reference_t<decltype(*i)>>::value,
  // "Not dereferenceable or uint8_t iterator");
  // static_assert(is_same<T, remove_reference_t<decltype(*j)>>::value,
  // "Output iterator does not dereference T");

  generate_n(j, count, [&i]() { return deserialize_number<T>(i); });
}

template <typename T, typename I, typename J>
void deserialize_objects(uint32_t count, I &i, J j) {
  generate_n(j, count, [&i]() {
    T t;
    deserializer<T>::deserialize(i, t);
    return t;
  });
}

template <typename T, typename I, typename J>
void deserialize_sequence(uint32_t count, I &i, J j, true_type) {
  deserialize_numbers<T>(count, i, j);
}

template <typename T, typename I, typename J>
void deserialize_sequence(uint32_t count, I &i, J j, false_type) {
  deserialize_objects<T>(count, i, j);
}

template <typename T, typename I, typename J>
void deserialize_sequence(I &i, J j) {
  auto count = deserialize_number<uint32_t>(i);
  deserialize_sequence<T>(count, i, j, is_arithmetic<T>{});
}

template <typename T, typename U, typename I, typename A>
void deserialize_associative(I &i, A &a) {
  auto count = deserialize_number<uint32_t>(i);
  vector<T> v_t;
  v_t.reserve(count);
  vector<U> v_u;
  v_u.reserve(count);
  deserialize_sequence<T>(count, i, back_inserter(v_t), is_arithmetic<T>{});
  deserialize_sequence<U>(count, i, back_inserter(v_u), is_arithmetic<U>{});
  a = inner_product(begin(v_t), end(v_t), begin(v_u), a,
                    [](A &a, pair<T, U> p) {
                      a.emplace(p);
                      return a;
                    },
                    [](T &t, U &u) { return make_pair(t, u); });
}

template <typename T, typename I> int deserialize_index(I &i, T &t) {
  deserializer<T>::deserialize(i, t);
  return 0;
}

template <typename I, typename T, size_t... Is>
void deserialize_heterogenous(I &i, T &t, index_sequence<Is...>) {
  using std::get;
  using n2w::get;
  for (auto rc : {deserialize_index(i, get<Is>(t))...})
    (void)rc;
}

template <typename T> struct deserializer {
  template <typename U = T, typename I>
  static auto deserialize(I &i, U &t) -> enable_if_t<is_arithmetic<U>::value> {
    t = deserialize_number<U>(i);
  }
  template <typename U = T, typename I>
  static auto deserialize(I &i, char16_t &t)
      -> enable_if_t<is_same<U, char16_t>::value> {
    struct cvt32 : codecvt<char32_t, char, mbstate_t> {};
    wstring_convert<cvt32, char32_t> cvter32;
    u32string c32s;
    c32s += deserialize_number<char32_t>(i);
    auto cs = cvter32.to_bytes(c32s);

    struct cvt16 : codecvt<char16_t, char, mbstate_t> {};
    wstring_convert<cvt16, char16_t> cvter16;
    auto ts = cvter16.from_bytes(cs);
    t = ts[0];
  }
  template <typename U = T, typename I>
  static auto deserialize(I &i, wchar_t &t)
      -> enable_if_t<is_same<U, wchar_t>::value> {
    char s[sizeof(char32_t)];
    auto n = min(utflen(*i), sizeof(s));
    copy_n(i, n, s);
    i += n;
    mbstate_t state;
    mbrtowc(&t, s, n, &state);
  }
  template <typename U = T, typename I>
  static auto deserialize(I &i, U &t) -> enable_if_t<is_enum<U>{}> {
    deserialize(i, static_cast<underlying_type_t<U> &>(t));
  }
};
template <typename T, size_t N> struct deserializer<T[N]> {
  template <typename I> static void deserialize(I &i, T (&t)[N]) {
    deserialize_sequence<T>(N, i, t, is_arithmetic<T>{});
  }
};
template <typename T, size_t M, size_t N> struct deserializer<T[M][N]> {
  template <typename I> static void deserialize(I &i, T (&t)[M][N]) {
    deserializer<T[M * N]>::deserialize(i, reinterpret_cast<T(&)[M * N]>(t));
  }
};
template <typename T, size_t N> struct deserializer<array<T, N>> {
  template <typename I> static void deserialize(I &i, array<T, N> &t) {
    deserialize_sequence<T>(N, i, begin(t), is_arithmetic<T>{});
  }
};
template <typename T, typename U> struct deserializer<pair<T, U>> {
  template <typename I> static void deserialize(I &i, pair<T, U> &t) {
    deserialize_heterogenous(i, t, make_index_sequence<2>{});
  }
};
template <typename T, typename... Ts> struct deserializer<tuple<T, Ts...>> {
  template <typename I> static void deserialize(I &i, tuple<T, Ts...> &t) {
    deserialize_heterogenous(i, t, make_index_sequence<sizeof...(Ts) + 1>{});
  }
};
template <typename T, typename... Traits>
struct deserializer<basic_string<T, Traits...>> {
  template <typename I>
  static void deserialize(I &i, basic_string<T, Traits...> &t) {
    string utf8;
    deserialize_sequence<char>(i, back_inserter(utf8));
    struct cvt : codecvt<T, char, mbstate_t> {};
    wstring_convert<cvt, T> cvter{"Could not convert from " +
                                  mangle<basic_string<char, Traits...>> +
                                  " to " + mangle<basic_string<T, Traits...>>};
    t = cvter.from_bytes(utf8);
  }
};
template <typename T, typename... Traits>
struct deserializer<vector<T, Traits...>> {
  template <typename I> static void deserialize(I &i, vector<T, Traits...> &t) {
    deserialize_sequence<T>(i, back_inserter(t));
  }
};
template <typename T, typename... Traits>
struct deserializer<list<T, Traits...>> {
  template <typename I> static void deserialize(I &i, list<T, Traits...> &t) {
    deserialize_sequence<T>(i, back_inserter(t));
  }
};
template <typename T, typename... Traits>
struct deserializer<forward_list<T, Traits...>> {
  template <typename I>
  static void deserialize(I &i, forward_list<T, Traits...> &t) {
    deserialize_sequence<T>(i, back_inserter(t));
  }
};
template <typename T, typename... Traits>
struct deserializer<deque<T, Traits...>> {
  template <typename I> static void deserialize(I &i, deque<T, Traits...> &t) {
    deserialize_sequence<T>(i, back_inserter(t));
  }
};
template <typename T, typename... Traits>
struct deserializer<set<T, Traits...>> {
  template <typename I> static void deserialize(I &i, set<T, Traits...> &t) {
    deserialize_sequence<T>(i, inserter(t, begin(t)));
  }
};
template <typename T, typename U, typename... Traits>
struct deserializer<map<T, U, Traits...>> {
  template <typename I> static void deserialize(I &i, map<T, U, Traits...> &t) {
    deserialize_associative<T, U>(i, t);
  }
};
template <typename T, typename... Traits>
struct deserializer<unordered_set<T, Traits...>> {
  template <typename I>
  static void deserialize(I &i, unordered_set<T, Traits...> &t) {
    deserialize_sequence<T>(i, inserter(t, begin(t)));
  }
};
template <typename T, typename U, typename... Traits>
struct deserializer<unordered_map<T, U, Traits...>> {
  template <typename I>
  static void deserialize(I &i, unordered_map<T, U, Traits...> &t) {
    deserialize_associative<T, U>(i, t);
  }
};
template <typename T, typename... Traits>
struct deserializer<multiset<T, Traits...>> {
  template <typename I>
  static void deserialize(I &i, multiset<T, Traits...> &t) {
    deserialize_sequence<T>(i, inserter(t, begin(t)));
  }
};
template <typename T, typename U, typename... Traits>
struct deserializer<multimap<T, U, Traits...>> {
  template <typename I>
  static void deserialize(I &i, multimap<T, U, Traits...> &t) {
    deserialize_associative<T, U>(i, t);
  }
};
template <typename T, typename... Traits>
struct deserializer<unordered_multiset<T, Traits...>> {
  template <typename I>
  static void deserialize(I &i, unordered_multiset<T, Traits...> &t) {
    deserialize_sequence<T>(i, inserter(t, begin(t)));
  }
};
template <typename T, typename U, typename... Traits>
struct deserializer<unordered_multimap<T, U, Traits...>> {
  template <typename I>
  static void deserialize(I &i, unordered_multimap<T, U, Traits...> &t) {
    deserialize_associative<T, U>(i, t);
  }
};
template <typename S, typename T, typename... Ts, typename... Bs>
struct deserializer<structure<S, tuple<T, Ts...>, tuple<Bs...>>> {
  template <typename B, typename I> static int deserialize(I &i, B &b) {
    deserializer<B>::deserialize(i, b);
    return 0;
  }
  template <typename I>
  static void deserialize(I &i,
                          structure<S, tuple<T, Ts...>, tuple<Bs...>> &t) {
    initializer_list<int> rc = {
        deserialize(i, *static_cast<Bs *>(t.s_write))...};
    (void)rc;
    deserialize_heterogenous(i, t, make_index_sequence<sizeof...(Ts) + 1>{});
  }
};

#define DESERIALIZE_SPEC(s, m, ...)                                            \
  namespace n2w {                                                              \
  template <> struct deserializer<s> {                                         \
    template <typename I> static void deserialize(I &i, s &_s) {               \
      CONSTRUCTOR(s, m, _s, __VA_ARGS__);                                      \
      deserializer<decltype(_s_v)>::deserialize(i, _s_v);                      \
    }                                                                          \
  };                                                                           \
  }
}
#endif