#ifndef _NATIVE_2_WEB_READWRITE_HPP_
#define _NATIVE_2_WEB_READWRITE_HPP_

#include "native-2-web-manglespec.hpp"
#include "native-2-web-deserialize.hpp"
#include "native-2-web-serialize.hpp"

#define READ_WRITE_SPEC(s, m)                                                  \
  MANGLE_SPEC(s, m);                                                           \
  SERIALIZE_SPEC(s, m);                                                        \
  DESERIALIZE_SPEC(s, m);

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
  return f(get<N>(args)...);
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
}

#endif