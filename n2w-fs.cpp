#include "native-2-web-plugin.hpp"

#include <experimental/filesystem>
#include <functional>
#include <iostream>
#include <string>

using namespace std;
using namespace experimental;
using namespace n2w;

N2W__JS_CONV(filesystem::path, vector<string>)
vector<string> path2vector(const filesystem::path &path) {
  auto abs_path = filesystem::absolute(path);
  vector<string> segments;
  transform(cbegin(abs_path), cend(abs_path), back_inserter(segments),
            mem_fn(&filesystem::path::generic_u8string));
  return segments;
}
filesystem::path vector2path(const vector<string> &segments) {
  filesystem::path path;
  path = accumulate(cbegin(segments), cend(segments), path, divides<>{});
  return path;
}
N2W__SERIALIZE_FROM(filesystem::path, path2vector);
N2W__DESERIALIZE_TO(std::vector<std::string>, vector2path);

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