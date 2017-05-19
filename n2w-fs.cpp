#include "native-2-web-plugin.hpp"

#include <experimental/filesystem>
#include <functional>
#include <iostream>
#include <string>

using namespace std;
using namespace std::experimental;
using namespace n2w;

auto current_working_directory() { return filesystem::current_path(); }
N2W__SERIALIZE_FROM(filesystem::path,
                    mem_fn(&filesystem::path::generic_u8string))
N2W__JS_FROM(filesystem::path, mem_fn(&filesystem::path::generic_u8string))

auto set_current_working_directory(string path) {
  cerr << "Setting current path: " << path << '\n';
  filesystem::current_path(path);
}

plugin plugin = []() {
  n2w::plugin plugin;
  plugin.register_service(DECLARE_API(current_working_directory), "");
  plugin.register_service(DECLARE_API(set_current_working_directory), "");
  return plugin;
}();