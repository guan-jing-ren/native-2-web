#ifndef _NATIVE_2_WEB_SERIALIZE_HPP_
#define _NATIVE_2_WEB_SERIALIZE_HPP_

#include "native-2-web-common.hpp"
#include "native-2-web-manglespec.hpp"
#include <codecvt>

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
  for (auto rc : initializer_list<int>{serialize_index(get<Is>(t), i)...})
    (void)rc;
}

template <typename T> struct serializer {
  template <typename I> static auto serialize(const T &t, I &i) {
    if
      constexpr(is_void_v<T> || is_same_v<T, void *>);
    else if
      constexpr(is_same_v<T, char16_t>) {
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
    else if
      constexpr(is_same_v<T, wchar_t>) {
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
    else if
      constexpr(is_enum_v<T>) serializer<underlying_type_t<T>>::serialize(
          static_cast<underlying_type_t<T>>(t), i);
    else if
      constexpr(is_arithmetic_v<T>) serialize_number(t, i);
    else if
      constexpr(is_heterogenous<T>)
          serialize_heterogenous(t, make_index_sequence<tuple_size_v<T>>{}, i);
    else if
      constexpr(is_sequence<T>)
          serialize_sequence<typename T::value_type>(t.size(), cbegin(t), i);
    else if
      constexpr(is_associative<T>)
          serialize_associative<typename T::key_type, typename T::mapped_type>(
              t, i);
  }
};
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
template <typename T, typename... Traits>
struct serializer<basic_string<T, Traits...>> {
  template <typename I>
  static void serialize(const basic_string<T, Traits...> &t, I &i) {
    string utf8;
    if
      constexpr(!is_same<T, char>{}) {
        wstring_convert<codecvt_utf8<T>, T> cvter{
            "Could not convert from " + mangled<basic_string<T, Traits...>>() +
            " to " + mangled<basic_string<char, Traits...>>()};
        utf8 = cvter.to_bytes(t);
      }
    else
      utf8 = t;
    serialize_sequence<char>(utf8.size(), cbegin(utf8), i);
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
template <typename T> struct serializer<optional<T>> {
  template <typename I> static void serialize(const optional<T> &o, I &i) {
    serializer<bool>::serialize(o, i);
    if (o)
      serializer<T>::serialize(*o, i);
  }
};
template <typename... Ts> struct serializer<variant<Ts...>> {
  template <typename I> static void serialize(const variant<Ts...> &v, I &i) {
    serializer<uint32_t>::serialize(v.index(), i);
    visit(
        [&i](const auto &t) {
          serializer<remove_cv_t<remove_reference_t<decltype(t)>>>::serialize(
              t, i);
        },
        v);
  }
};
template <typename R, intmax_t N, intmax_t D>
struct serializer<chrono::duration<R, ratio<N, D>>> {
  template <typename I>
  static void serialize(const chrono::duration<R, ratio<N, D>> &t, I &i) {
    serializer<double>::serialize(t.count(), i);
  }
};
template <typename C, typename D> struct serializer<chrono::time_point<C, D>> {
  template <typename I>
  static void serialize(const chrono::time_point<C, D> &t, I &i) {
    serializer<D>::serialize(t.time_since_epoch(), i);
  }
};
template <typename T> struct serializer<complex<T>> {
  template <typename I> static void serialize(const complex<T> &c, I &i) {
    serializer<pair<T, T>>::serialize(make_pair(c.real(), c.imag()), i);
  }
};
template <typename T> struct serializer<atomic<T>> {
  template <typename I> static void serialize(const atomic<T> &a, I &i) {
    serializer<T>::serialize(a, i);
  }
};
template <size_t N> struct serializer<bitset<N>> {
  template <typename I> static void serialize(const bitset<N> &b, I &i) {
    serializer<string>::serialize(b.to_string(), i);
  }
};
template <> struct serializer<filesystem::space_info> {
  template <typename I>
  static void serialize(const filesystem::space_info &s, I &i) {
    serializer<tuple<double, double, double>>::serialize(
        make_tuple(s.capacity, s.free, s.available), i);
  }
};
template <> struct serializer<filesystem::file_status> {
  template <typename I>
  static void serialize(const filesystem::file_status &f, I &i) {
    serializer<pair<filesystem::file_type, filesystem::perms>>::serialize(
        make_pair(f.type(), f.permissions()), i);
  }
};
template <> struct serializer<filesystem::path> {
  template <typename I> static void serialize(const filesystem::path &p, I &i) {
    vector<string> segments;
    transform(cbegin(p), cend(p), back_inserter(segments),
              std::mem_fn(&filesystem::path::generic_u8string));
    serializer<decltype(segments)>::serialize(segments, i);
  }
};
template <> struct serializer<filesystem::directory_entry> {
  template <typename I>
  static void serialize(const filesystem::directory_entry &d, I &i) {
    error_code ec;
    serializer<
        tuple<filesystem::path, bool, uint32_t, uint32_t,
              decltype(filesystem::last_write_time(d).time_since_epoch()),
              filesystem::file_status>>::
        serialize(
            make_tuple(d.path(), filesystem::exists(d, ec),
                       filesystem::file_size(d.path(), ec),
                       filesystem::hard_link_count(d.path(), ec),
                       filesystem::last_write_time(d, ec).time_since_epoch(),
                       d.symlink_status(ec)),
            i);
    // serializer<bool>::serialize(d.exists(), i);
    // serializer<uint32_t>::serialize(d.file_size(), i);
    // serializer<uint32_t>::serialize(d.hard_link_count(),
    //                                                      i);
    // serializer<decltype(d.last_write_time().time_since_epoch())>::serialize(
    //     d.last_write_time().time_since_epoch(), i);
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

#define N2W__SERIALIZE_FROM(s, c)                                              \
  namespace n2w {                                                              \
  template <> struct serializer<s> {                                           \
    template <typename I> static void serialize(const s &_s, I &i) {           \
      serializer<decltype(c(_s))>::serialize(c(_s), i);                        \
    }                                                                          \
  };                                                                           \
  }
}
#endif