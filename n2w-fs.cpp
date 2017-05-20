#include "native-2-web-plugin.hpp"

#include <experimental/filesystem>
#include <functional>
#include <iostream>
#include <string>

using namespace std;
using namespace std::experimental;
using namespace n2w;

N2W__JS_CONV(filesystem::path, string)
N2W__SERIALIZE_FROM(filesystem::path,
                    mem_fn(&filesystem::path::generic_u8string))
N2W__DESERIALIZE_TO(string, filesystem::path);

auto current_working_directory() { return filesystem::current_path(); }

auto set_current_working_directory(const filesystem::path &path) {
  cerr << "Setting current path: " << path << '\n';
  filesystem::current_path(path);
}

plugin plugin = []() {
  n2w::plugin plugin;
  plugin.register_service(DECLARE_API(current_working_directory), "");
  plugin.register_service(DECLARE_API(set_current_working_directory), "");
  return plugin;
}();