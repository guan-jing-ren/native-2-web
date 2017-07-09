#include "native-2-web-plugin.hpp"

#include <iostream>

using namespace std;
using namespace experimental;
using namespace n2w;

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