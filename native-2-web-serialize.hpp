#ifndef _NATIVE_2_WEB_SERIALIZE_HPP_
#define _NATIVE_2_WEB_SERIALIZE_HPP_

#include "native-2-web-common.hpp"
#include "native-2-web-manglespec.hpp"
#include <locale>

namespace n2w {
using namespace std;

template <typename T> struct serializer;
template <typename T, typename I> I serialize(const T &t, I i) {
  serializer<T>::serialize(t, i);
  return i;
}

template <typename T, typename I> void serialize_number(T t, I &i) {
  static_assert(is_arithmetic<T>::value, "Not an arithmetic type");
  // static_assert(is_same<uint8_t, remove_reference_t<decltype(*i)>>::value,
  // "Not dereferenceable or uint8_t iterator");

  i = copy_n(reinterpret_cast<uint8_t *>(&t), serial_size<T>, i);
}

template <typename T, typename I, typename J>
void serialize_numbers(uint32_t count, J j, I &i) {
  static_assert(is_arithmetic<T>::value, "Not an arithmetic type");
  // static_assert(is_same<uint8_t, remove_reference_t<decltype(*i)>>::value,
  // "Not dereferenceable or uint8_t iterator");
  // static_assert(is_same<T, remove_reference_t<decltype(*j)>>::value,
  // "Output iterator does not dereference T");

  for_each(j, j + count, [&i](const T &t) { serialize_number<T>(t, i); });
}

template <typename T, typename I, typename J>
void serialize_objects(uint32_t count, J j, I &i) {
  for (auto end = count, c = 0u; c < end; ++c)
    serializer<T>::serialize(*j++, i);
}

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
  vector<T> v_t;
  v_t.reserve(a.size());
  vector<U> v_u;
  v_u.reserve(a.size());
  transform(cbegin(a), cend(a), back_inserter(v_t),
            [](const pair<T, U> &p) { return p.first; });
  transform(cbegin(a), cend(a), back_inserter(v_u),
            [](const pair<T, U> &p) { return p.second; });
  serialize_sequence<T>(a.size(), cbegin(v_t), i);
  serialize_sequence_bounded<U>(a.size(), cbegin(v_u), i);
}

template <typename T, typename I> int serialize_index(const T &t, I &i) {
  serializer<T>::serialize(t, i);
  return 0;
}

template <typename I, typename T, size_t... Is>
void serialize_heterogenous(const T &t, index_sequence<Is...>, I &i) {
  using std::get;
  using n2w::get;
  for (auto rc : {serialize_index(get<Is>(t), i)...})
    (void)rc;
}

template <typename T> struct serializer {
  template <typename U = T, typename I>
  static auto serialize(const U t, I &i)
      -> enable_if_t<is_arithmetic<U>::value> {
    serialize_number(t, i);
  }
  template <typename U = T, typename I>
  static auto serialize(const char16_t t, I &i)
      -> enable_if_t<is_same<U, char16_t>::value> {
    u16string ts;
    ts += t;
    struct cvt16 : codecvt<char16_t, char, mbstate_t> {};
    wstring_convert<cvt16, char16_t> cvter16;
    auto cs = cvter16.to_bytes(ts);
    struct cvt32 : codecvt<char32_t, char, mbstate_t> {};
    wstring_convert<cvt32, char32_t> cvter32;
    auto c32s = cvter32.from_bytes(cs);
    serialize_number<char32_t>(c32s[0], i);
  }
  template <typename U = T, typename I>
  static auto serialize(const wchar_t t, I &i)
      -> enable_if_t<is_same<U, wchar_t>::value> {
    wstring ts;
    ts += t;
    struct wcvt : codecvt<wchar_t, char, mbstate_t> {};
    wstring_convert<wcvt, wchar_t> wcvter;
    auto cs = wcvter.to_bytes(ts);
    struct cvt32 : codecvt<char32_t, char, mbstate_t> {};
    wstring_convert<cvt32, char32_t> cvter32;
    auto c32s = cvter32.from_bytes(cs);
    serialize_number<char32_t>(c32s[0], i);
  }
  template <typename U = T, typename I>
  static auto serialize(const U t, I &i) -> enable_if_t<is_enum<U>{}> {
    serialize(static_cast<underlying_type_t<U>>(t), i);
  }
};
template <> struct serializer<void> {};
template <typename T, size_t N> struct serializer<T[N]> {
  template <typename I> static void serialize(const T (&t)[N], I &i) {
    serialize_sequence_bounded<T>(N, t, i);
  }
};
template <typename T, size_t M, size_t N> struct serializer<T[M][N]> {
  template <typename I> static void serialize(const T (&t)[M][N], I &i) {
    serializer<T[M * N]>::serialize(reinterpret_cast<const T(&)[M * N]>(t), i);
  }
};
template <typename T, size_t N> struct serializer<array<T, N>> {
  template <typename I> static void serialize(const array<T, N> &t, I &i) {
    serialize_sequence_bounded<T>(N, cbegin(t), i);
  }
};
template <typename T, typename U> struct serializer<pair<T, U>> {
  template <typename I> static void serialize(const pair<T, U> &t, I &i) {
    serialize_heterogenous(t, make_index_sequence<2>{}, i);
  }
};
template <typename T, typename... Ts> struct serializer<tuple<T, Ts...>> {
  template <typename I> static void serialize(const tuple<T, Ts...> &t, I &i) {
    serialize_heterogenous(t, make_index_sequence<sizeof...(Ts) + 1>{}, i);
  }
};
template <typename T, typename... Traits>
struct serializer<basic_string<T, Traits...>> {
  template <typename I>
  static void serialize(const basic_string<T, Traits...> &t, I &i) {
    struct cvt : codecvt<T, char, mbstate_t> {};
    wstring_convert<cvt, T> cvter{"Could not convert from " +
                                  mangle<basic_string<T, Traits...>> + " to " +
                                  mangle<basic_string<char, Traits...>>};
    string utf8 = cvter.to_bytes(t);
    serialize_sequence<char>(utf8.size(), cbegin(utf8), i);
  }
};
template <typename T, typename... Traits>
struct serializer<vector<T, Traits...>> {
  template <typename I>
  static void serialize(const vector<T, Traits...> &t, I &i) {
    serialize_sequence<T>(t.size(), cbegin(t), i);
  }
};
template <typename T, typename... Traits>
struct serializer<list<T, Traits...>> {
  template <typename I>
  static void serialize(const list<T, Traits...> &t, I &i) {
    serialize_sequence<T>(t.size(), cbegin(t), i);
  }
};
template <typename T, typename... Traits>
struct serializer<forward_list<T, Traits...>> {
  template <typename I>
  static void serialize(const forward_list<T, Traits...> &t, I &i) {
    serialize_sequence<T>(t.size(), cbegin(t), i);
  }
};
template <typename T, typename... Traits>
struct serializer<deque<T, Traits...>> {
  template <typename I>
  static void serialize(const deque<T, Traits...> &t, I &i) {
    serialize_sequence<T>(t.size(), cbegin(t), i);
  }
};
template <typename T, typename... Traits> struct serializer<set<T, Traits...>> {
  template <typename I>
  static void serialize(const set<T, Traits...> &t, I &i) {
    serialize_sequence<T>(t.size(), cbegin(t), i);
  }
};
template <typename T, typename U, typename... Traits>
struct serializer<map<T, U, Traits...>> {
  template <typename I>
  static void serialize(const map<T, U, Traits...> &t, I &i) {
    serialize_associative<T, U>(t, i);
  }
};
template <typename T, typename... Traits>
struct serializer<unordered_set<T, Traits...>> {
  template <typename I>
  static void serialize(const unordered_set<T, Traits...> &t, I &i) {
    serialize_sequence<T>(t.size(), cbegin(t), i);
  }
};
template <typename T, typename U, typename... Traits>
struct serializer<unordered_map<T, U, Traits...>> {
  template <typename I>
  static void serialize(const unordered_map<T, U, Traits...> &t, I &i) {
    serialize_associative<T, U>(t, i);
  }
};
template <typename T, typename... Traits>
struct serializer<multiset<T, Traits...>> {
  template <typename I>
  static void serialize(const multiset<T, Traits...> &t, I &i) {
    serialize_sequence<T>(t.size(), cbegin(t), i);
  }
};
template <typename T, typename U, typename... Traits>
struct serializer<multimap<T, U, Traits...>> {
  template <typename I>
  static void serialize(const multimap<T, U, Traits...> &t, I &i) {
    serialize_associative<T, U>(t, i);
  }
};
template <typename T, typename... Traits>
struct serializer<unordered_multiset<T, Traits...>> {
  template <typename I>
  static void serialize(const unordered_multiset<T, Traits...> &t, I &i) {
    serialize_sequence<T>(t.size(), cbegin(t), i);
  }
};
template <typename T, typename U, typename... Traits>
struct serializer<unordered_multimap<T, U, Traits...>> {
  template <typename I>
  static void serialize(const unordered_multimap<T, U, Traits...> &t, I &i) {
    serialize_associative<T, U>(t, i);
  }
};
template <typename S, typename T, typename... Ts, typename... Bs>
struct serializer<structure<S, tuple<T, Ts...>, tuple<Bs...>>> {
  template <typename B, typename I> static int serialize(const B &b, I &i) {
    serializer<B>::serialize(b, i);
    return 0;
  }
  template <typename I>
  static void serialize(const structure<S, tuple<T, Ts...>, tuple<Bs...>> &t,
                        I &i) {
    initializer_list<int> rc = {
        serialize(*static_cast<const Bs *>(t.s_read), i)...};
    (void)rc;
    serialize_heterogenous(t, make_index_sequence<sizeof...(Ts) + 1>{}, i);
  }
};

#define N2W__SERIALIZE_SPEC(s, m, ...)                                         \
  namespace n2w {                                                              \
  template <> struct serializer<s> {                                           \
    template <typename I> static void serialize(const s &_s, I &i) {           \
      N2W__CONSTRUCTOR(s, m, _s, __VA_ARGS__);                                 \
      serializer<decltype(_s_v)>::serialize(_s_v, i);                          \
    }                                                                          \
  };                                                                           \
  }
}
#endif