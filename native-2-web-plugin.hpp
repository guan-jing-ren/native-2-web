#ifndef _NATIVE_2_WEB_PLUGIN_HPP_
#define _NATIVE_2_WEB_PLUGIN_HPP_

#include "../fundamental-machines/basic_plugin.hpp"
#include "native-2-web-js.hpp"
#include "native-2-web-readwrite.hpp"

#include <boost/preprocessor.hpp>
#include <experimental/filesystem>
#include <regex>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace n2w {
namespace detail {
class plugin_impl {
protected:
  using buf_type = std::vector<uint8_t>;
  template <typename... Args>
  using args_t =
      std::conditional_t<sizeof...(Args), std::tuple<Args...>, void *>;

  std::unordered_map<std::string, std::string> pointer_to_name;
  std::unordered_map<std::string, std::string> name_to_readable;
  std::unordered_map<std::string, std::string> pointer_to_description;
  std::unordered_map<std::string, std::function<buf_type(const buf_type &)>>
      pointer_to_function;
  std::unordered_map<std::string, std::string> pointer_to_javascript;

  std::unordered_set<std::string> services;
  std::unordered_set<std::string> push_notifiers;
  std::unordered_set<std::string> kaonashis;
};
}

class plugin : private basic_plugin, public detail::plugin_impl {

  template <typename R, typename... Args, std::size_t... Is>
  static auto create_caller(R (*callback)(Args...),
                            std::index_sequence<Is...>) {
    using args_type = args_t<Args...>;
    auto reader = [](const buf_type &in) -> args_type {
      args_type args;
      deserialize(cbegin(in), args);
      return args;
    };
    auto writer = [](const R &return_val) -> buf_type {
      buf_type buf;
      serialize(return_val, back_inserter(buf));
      return buf;
    };
    auto caller = [reader, writer, callback](const buf_type &in) -> buf_type {
      return writer(callback(get<Is>(reader(in))...));
    };
    return caller;
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

  template <typename R, typename... Args>
  void register_api(const char *name, R (*callback)(Args...),
                    const char *description) {
    const auto pointer = function_address(callback);
    pointer_to_name[pointer] = name;
    pointer_to_description[pointer] = description;
    auto caller =
        create_caller(callback, std::make_index_sequence<sizeof...(Args)>{});
    pointer_to_function[pointer] = caller;
  }

public:
  plugin() : basic_plugin(nullptr) {}

  // TODO: Register service.
  // TODO: Register push notifier.
  // TODO: Register kaonashi.

  template <typename R, typename... Args>
  void register_service(const char *name, R (*callback)(Args...),
                        const char *description) {
    register_api(name, callback, description);
    services.emplace(function_address(callback));
    pointer_to_javascript[function_address(callback)] =
        R"(this.)" + string{name} + R"( = create_service(')" +
        std::regex_replace(function_address(callback), regex{"'"}, R"(\')") +
        R"(', )" + to_js<args_t<Args...>>::create_writer() + R"(, )" +
        to_js<R>::create_reader() + R"();)";
  }
  template <typename R, typename... Args>
  void register_push_notifier(const char *name, R (*callback)(Args...),
                              const char *description) {
    register_api(name, callback, description);
    push_notifiers.emplace(function_address(callback));
    to_js<args_t<Args...>>::create_writer();
    to_js<R>::create_reader();
  }
  template <typename R, typename... Args>
  void register_kaonashi(const char *name, R (*callback)(Args...),
                         const char *description) {
    register_api(name, callback, description);
    kaonashis.emplace(function_address(callback));
    to_js<args_t<Args...>>::create_writer();
  }

  std::vector<std::string> get_services() const {
    return std::vector<std::string>{cbegin(services), cend(services)};
  }
  std::vector<std::string> get_push_notifiers() const {
    return std::vector<std::string>{cbegin(push_notifiers),
                                    cend(push_notifiers)};
  }
  std::vector<std::string> get_kaonashis() const {
    return std::vector<std::string>{cbegin(kaonashis), cend(kaonashis)};
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
        plugin_impl(static_cast<plugin_impl &>(sym<plugin>("plugin"))) {}
};

#define DECLARE_API(x) #x, x
}
#endif