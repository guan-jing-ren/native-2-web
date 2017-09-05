#ifndef _NATIVE_2_WEB_PLUGIN_HPP_
#define _NATIVE_2_WEB_PLUGIN_HPP_

#include "../fundamental-machines/basic_plugin.hpp"
#include "native-2-web-js.hpp"
#include "native-2-web-readwrite.hpp"

#include <experimental/filesystem>
#include <regex>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace n2w {
namespace detail {

template <typename F> struct func;
template <typename Ret, typename... Args> struct func<Ret(Args...)> {
  using args_t = std::conditional_t<(sizeof...(Args) > 0),
                                    std::tuple<decay_t<Args>...>, void *>;
  using ret_t = std::conditional_t<std::is_void<Ret>{}, void *, Ret>;
  static constexpr auto indices = std::make_index_sequence<sizeof...(Args)>{};
  static auto function_address(const char *name) {
    return n2w::function_address(name, static_cast<Ret (*)(Args...)>(nullptr));
  };
};
template <typename Ret, typename... Args>
struct func<Ret (*)(Args...)> : func<Ret(Args...)> {};
template <typename Ret, typename... Args>
struct func<Ret (&)(Args...)> : func<Ret(Args...)> {};
template <typename Ret, typename T, typename... Args>
struct func<Ret (T::*)(Args...)> : func<Ret(Args...)> {};
template <typename Ret, typename T, typename... Args>
struct func<Ret (T::*)(Args...) const> : func<Ret(Args...)> {};
template <typename Ret, typename T, typename... Args>
struct func<Ret (T::*)(Args...) volatile> : func<Ret(Args...)> {};
template <typename Ret, typename T, typename... Args>
struct func<Ret (T::*)(Args...) const volatile> : func<Ret(Args...)> {};
template <typename F> struct func : func<decltype(&decay_t<F>::operator())> {};

class plugin_impl {
protected:
  using buf_type = std::vector<uint8_t>;
  template <typename F> using args_t = typename func<F>::args_t;
  template <typename F> using ret_t = typename func<F>::ret_t;

  std::unordered_map<std::string, std::string> pointer_to_name;
  std::unordered_map<std::string, std::string> name_to_readable;
  std::unordered_map<std::string, std::string> pointer_to_description;
  std::unordered_map<std::string, std::function<buf_type(const buf_type &)>>
      pointer_to_function;
  std::unordered_map<std::string, std::string> pointer_to_javascript;
  std::unordered_map<std::string, std::string> pointer_to_generator;

  std::unordered_set<std::string> services;
  std::unordered_set<std::string> push_notifiers;
  std::unordered_set<std::string> kaonashis;
};
}

class plugin : private basic_plugin, public detail::plugin_impl {
  template <std::size_t... Is, typename R, typename W, typename C>
  static auto generic_caller(R &&reader, W &&writer, C &&callback) {
    return [reader, writer, callback](const buf_type &in) mutable -> buf_type {
      if
        constexpr(std::is_same_v<ret_t<C>, void *>) {
          callback(get<Is>(reader(in))...);
          return writer(nullptr);
        }
      else
        return writer(callback(get<Is>(reader(in))...));
    };
  }
  template <typename F, std::size_t... Is>
  static auto create_caller(F &&callback, std::index_sequence<Is...>) {
    using args_type = args_t<F>;
    using ret_type = ret_t<F>;
    auto reader = [](const buf_type &in) -> args_type {
      args_type args;
      deserialize(cbegin(in), args);
      return args;
    };
    auto writer = [](const ret_t<F> &return_val) -> buf_type {
      buf_type buf;
      serialize(return_val, back_inserter(buf));
      return buf;
    };
    return generic_caller<Is...>(reader, writer, callback);
  }

  // cd, search through names, then descriptions, using the following strategy:
  // Exact match, starting from the beginning.
  // Exact match, starting from anywhere.
  // Starting from the beginning, first letter of every 'word'.
  // Starting from the beginning, wildcards between letters.
  // Starting from anywhere, first letter of every 'word'.
  // Starting from anywhere, wildcards between letters.

  // Permute wildcards from '.?' to '.*?' to '.*'.
  // Highlight the matching characters.
  // Suggest next character to quickly differentiate.
  // Case insensitive.

  // TODO: Push notifications on same websocket requires all services to
  // generate request ids to differentiate between requests and notifications.

  template <typename F>
  void register_api(const char *name, F &&callback, const char *description) {
    const auto pointer = detail::func<F>::function_address(name);
    pointer_to_name[pointer] = name;
    pointer_to_description[pointer] = description;
    auto caller = create_caller(callback, detail::func<F>::indices);
    pointer_to_function[pointer] = caller;
  }

public:
  plugin() : basic_plugin(nullptr) {}

  // TODO: Register service.
  // TODO: Register push notifier.
  // TODO: Register kaonashi.

  template <typename F>
  void register_service(const char *name, F &&callback,
                        const char *description) {
    register_api(name, callback, description);
    services.emplace(detail::func<F>::function_address(name));
    pointer_to_javascript[detail::func<F>::function_address(name)] =
        R"(create_service(')" +
        std::regex_replace(detail::func<F>::function_address(name), regex{"'"},
                           R"(\')") +
        R"(', )" + to_js<args_t<F>>::create_writer() + R"(, )" +
        to_js<ret_t<F>>::create_reader() + R"())";

    pointer_to_generator[detail::func<F>::function_address(name)] =
        R"(function (parent, executor) {
  let html_args = )" +
        to_js<args_t<F>>::create_html() + R"(;
  let html_return = )" +
        to_js<ret_t<F>>::create_html() + R"(;
  (this.html_function || html_function)(parent, html_args, html_return, ')" +
        name + R"(', executor);
})";
  }
  template <typename F>
  void register_push_notifier(const char *name, F &&callback,
                              const char *description) {
    register_api(name, callback, description);
    push_notifiers.emplace(detail::func<F>::function_address(name));
    to_js<args_t<F>>::create_writer();
    to_js<ret_t<F>>::create_reader();
  }
  template <typename F>
  void register_kaonashi(const char *name, F &&callback,
                         const char *description) {
    register_api(name, callback, description);
    kaonashis.emplace(detail::func<F>::function_address(name));
    to_js<args_t<F>>::create_writer();
  }

  std::vector<std::string> get_services() const {
    return {cbegin(services), cend(services)};
  }
  std::vector<std::string> get_push_notifiers() const {
    return {cbegin(push_notifiers), cend(push_notifiers)};
  }
  std::vector<std::string> get_kaonashis() const {
    return {cbegin(kaonashis), cend(kaonashis)};
  }

  std::string get_name(std::string pointer) { return pointer_to_name[pointer]; }

  std::string get_generator(std::string pointer) {
    return pointer_to_generator[pointer];
  }

  std::string get_javascript(std::string pointer) {
    return pointer_to_javascript[pointer];
  }

  std::reference_wrapper<const std::function<buf_type(const buf_type &)>>
  get_function(const std::string &pointer) {
    return std::cref(pointer_to_function[pointer]);
  }

  plugin(const char *dll)
      : basic_plugin(dll),
        plugin_impl(static_cast<plugin_impl>(sym<plugin>("plugin"))) {}
};

#define N2W__DECLARE_API(x) #x, x
}
#endif