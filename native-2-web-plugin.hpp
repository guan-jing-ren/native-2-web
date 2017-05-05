#ifndef _NATIVE_2_WEB_PLUGIN_HPP_
#define _NATIVE_2_WEB_PLUGIN_HPP_

#include "../fundamental-machines/basic_plugin.hpp"
#include "native-2-web-js.hpp"
#include "native-2-web-readwrite.hpp"

#include <experimental/filesystem>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace n2w {
class plugin : basic_plugin {
  using buf_type = std::vector<uint8_t>;

  template <typename Reader, typename Writer, typename R, typename... Args,
            std::size_t... Is>
  auto create_caller(R (*callback)(Args...), std::index_sequence<Is...>) {
    using namespace std;
    using args_type = tuple<Args...>;
    auto reader = [](const buf_type &in) -> args_type {
      args_type args;
      n2w::deserialize<args_type>(cbegin(in), args);
      return args;
    };
    auto writer = [](const R &return_val) -> buf_type {
      buf_type buf;
      n2w::serialize<R>(return_val, back_inserter(buf));
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

  std::unordered_map<std::string, std::string> pointer_to_name;
  std::unordered_map<std::string, std::string> name_to_readable;
  std::unordered_map<std::string, std::string> pointer_to_description;
  std::unordered_map<std::string, std::function<buf_type(const buf_type &)>>
      pointer_to_function;
  std::unordered_map<std::string, std::string> pointer_to_javascript;

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

  std::unordered_set<std::string> services;
  std::unordered_set<std::string> pusher_notifiers;
  std::unordered_set<std::string> kaonashis;

public:
  n2w_plugin() : basic_plugin(nullptr) {}

  // TODO: Register service.
  // TODO: Register push notifier.
  // TODO: Register kaonashi.
};
}

#endif