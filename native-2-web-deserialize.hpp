#ifndef _NATIVE_2_WEB_DESERIALIZE_HPP_
#define _NATIVE_2_WEB_DESERIALIZE_HPP_

#include "native-2-web-common.hpp"
#include "native-2-web-manglespec.hpp"
#include <codecvt>

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
  for (auto rc : initializer_list<int>{deserialize_index(i, get<Is>(t))...})
    (void)rc;
}

template <typename T> struct deserializer {
  template <typename I> static auto deserialize(I &i, T &t) {
    if
      constexpr(is_void_v<T> || is_same_v<T, void *>);
    else if
      constexpr(is_same_v<T, char16_t>) {
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
    else if
      constexpr(is_same_v<T, wchar_t>) {
        char s[sizeof(char32_t)];
        auto n = min(utflen(*i), sizeof(s));
        copy_n(i, n, s);
        i += n;
        mbstate_t state;
        mbrtowc(&t, s, n, &state);
      }
    else if
      constexpr(is_enum_v<T>) {
        underlying_type_t<T> u;
        deserializer<decltype(u)>::deserialize(i, u);
        t = static_cast<T>(u);
      }
    else if
      constexpr(is_arithmetic_v<T>) t = deserialize_number<T>(i);
    else if
      constexpr(is_heterogenous<T>) deserialize_heterogenous(
          i, t, make_index_sequence<tuple_size_v<T>>{});
    else if
      constexpr(is_sequence<T>) {
        if
          constexpr(is_pushback_sequence<T>)
              deserialize_sequence<typename T::value_type>(i, back_inserter(t));
        else
          deserialize_sequence<typename T::value_type>(i, inserter(t, end(t)));
      }
    else if
      constexpr(is_associative<T>)
          deserialize_associative<typename T::key_type,
                                  typename T::mapped_type>(i, t);
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
template <typename T, typename... Traits>
struct deserializer<basic_string<T, Traits...>> {
  template <typename I>
  static void deserialize(I &i, basic_string<T, Traits...> &t) {
    string utf8;
    deserialize_sequence<char>(i, back_inserter(utf8));
    if
      constexpr(!is_same<T, char>{}) {
        wstring_convert<codecvt_utf8<T>, T> cvter{
            "Could not convert from " +
            mangled<basic_string<char, Traits...>>() + " to " +
            mangled<basic_string<T, Traits...>>()};
        t = cvter.from_bytes(utf8);
      }
    else
      t = utf8;
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
template <typename T> struct deserializer<optional<T>> {
  template <typename I> static void deserialize(I &i, optional<T> &o) {
    bool b;
    deserializer<bool>::deserialize(i, b);
    if (b) {
      o = T{};
      deserializer<T>::deserialize(i, *o);
    }
  }
};
template <typename... Ts> struct deserializer<variant<Ts...>> {
  template <decltype(variant_npos) N>
  static int init_variant(uint32_t i, variant<Ts...> &v) {
    if (i != N)
      return 0;
    v = variant_alternative_t<N, variant<Ts...>>{};
    return 0;
  }
  template <size_t... Ns>
  static void init_variant(uint32_t i, variant<Ts...> &v,
                           index_sequence<Ns...>) {
    initializer_list<int> rc = {init_variant<Ns>(i, v)...};
    (void)rc;
  }
  template <typename I> static void deserialize(I &i, variant<Ts...> &v) {
    uint32_t index = static_cast<uint32_t>(variant_npos);
    deserializer<uint32_t>::deserialize(i, index);
    init_variant(index, v, make_index_sequence<sizeof...(Ts)>{});
    visit(
        [&i](auto &t) {
          deserializer<remove_reference_t<decltype(t)>>::deserialize(i, t);
        },
        v);
  }
};
template <typename R, intmax_t N, intmax_t D>
struct deserializer<chrono::duration<R, ratio<N, D>>> {
  template <typename I>
  static void deserialize(I &i, chrono::duration<R, ratio<N, D>> &t) {
    double r;
    deserializer<double>::deserialize(i, r);
    t = chrono::duration<R, ratio<N, D>>{static_cast<R>(r)};
  }
};
template <typename C, typename D>
struct deserializer<chrono::time_point<C, D>> {
  template <typename I>
  static void deserialize(I &i, chrono::time_point<C, D> &t) {
    auto d = remove_reference_t<decltype(t)>::min().time_since_epoch();
    deserializer<D>::deserialize(i, d);
    t = remove_reference_t<decltype(t)>{d};
  }
};
template <typename T> struct deserializer<complex<T>> {
  template <typename I> static void deserialize(I &i, complex<T> &c) {
    T r, j;
    deserializer<T>::deserialize(i, r);
    deserializer<T>::deserialize(i, j);
    c = complex<T>{r, j};
  }
};
template <typename T> struct deserializer<atomic<T>> {
  template <typename I> static void deserialize(I &i, atomic<T> &a) {
    T t;
    deserializer<T>::deserialize(i, t);
    a = t;
  }
};
template <size_t N> struct deserializer<bitset<N>> {
  template <typename I> static void deserialize(I &i, bitset<N> &b) {
    string s;
    deserializer<string>::deserialize(i, s);
    b = bitset<N>{s};
  }
};
template <> struct deserializer<filesystem::space_info> {
  template <typename I>
  static void deserialize(I &i, filesystem::space_info &s) {
    deserializer<double>::deserialize(i, s.capacity);
    deserializer<double>::deserialize(i, s.free);
    deserializer<double>::deserialize(i, s.available);
  }
};
template <> struct deserializer<filesystem::file_status> {
  template <typename I>
  static void deserialize(I &i, filesystem::file_status &f) {
    decltype(f.type()) type;
    decltype(f.permissions()) permissions;
    deserializer<decltype(type)>::deserialize(i, type);
    deserializer<decltype(permissions)>::deserialize(i, permissions);
    f.type(type);
    f.permissions(permissions);
  }
};
template <> struct deserializer<filesystem::path> {
  template <typename I> static void deserialize(I &i, filesystem::path &p) {
    vector<string> segments;
    deserializer<decltype(segments)>::deserialize(i, segments);
    p = accumulate(cbegin(segments), cend(segments), filesystem::path{},
                   divides<>{});
  }
};
template <> struct deserializer<filesystem::directory_entry> {
  template <typename I>
  static void deserialize(I &i, filesystem::directory_entry &d) {
    filesystem::path path;
    deserializer<filesystem::path>::deserialize(i, path);
    bool exists;
    // deserializer<bool>::deserialize(i,exists);
    deserializer<bool>::deserialize(i, exists);
    decltype(filesystem::file_size(d.path())) file_size;
    // deserializer<uint32_t>::deserialize(i,file_size);
    deserializer<uint32_t>::deserialize(i, file_size);
    uint32_t hard_link_count;
    // deserializer<uint32_t>::deserialize(i, hard_link_count, i);
    deserializer<uint32_t>::deserialize(i, hard_link_count);
    decltype(
        filesystem::last_write_time(d).time_since_epoch()) time_since_epoch;
    // deserializer<decltype(d.last_write_time().time_since_epoch())>::deserialize(i,
    //     time_since_epoch);
    deserializer<decltype(filesystem::last_write_time(d).time_since_epoch())>::
        deserialize(i, time_since_epoch);
    filesystem::file_status symlink_status;
    deserializer<filesystem::file_status>::deserialize(i, symlink_status);
    d = filesystem::directory_entry(path);
  }
};

#define N2W__DESERIALIZE_SPEC(s, m, ...)                                       \
  namespace n2w {                                                              \
  template <> struct deserializer<s> {                                         \
    template <typename I> static void deserialize(I &i, s &_s) {               \
      N2W__CONSTRUCTOR(s, m, _s, __VA_ARGS__);                                 \
      deserializer<decltype(_s_v)>::deserialize(i, _s_v);                      \
    }                                                                          \
  };                                                                           \
  }

#define N2W__DESERIALIZE_TO(s, c)                                              \
  namespace n2w {                                                              \
  template <> struct deserializer<decltype(c(s{}))> {                          \
    template <typename I>                                                      \
    static void deserialize(I &i, decltype(c(s{})) &_c) {                      \
      s _s;                                                                    \
      deserializer<s>::deserialize(i, _s);                                     \
      _c = c(_s);                                                              \
    }                                                                          \
  };                                                                           \
  }
}
#endif