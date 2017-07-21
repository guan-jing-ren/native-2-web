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

// Options:
// - Show . and ..
// - Show hidden files
// - Show special files
// - Column order, sort order, natural sorting (alpha vs numeric vs symbolic
// 'fields')
// - Directories first in list or don't separate directories
// - Directory size if directory
// - Filter names and properties
// - Recursive
// - Directory options
auto list_files(optional<vector<filesystem::path>> paths) {
  cerr << "Listing files\n";
  vector<filesystem::directory_entry> list;
  if (paths)
    for (auto &&path : *paths)
      for (auto &&entry : filesystem::directory_iterator{path})
        list.push_back(entry);
  else
    for (auto &&entry :
         filesystem::directory_iterator{current_working_directory()}) {
      cerr << entry.path() << '\n';
      list.push_back(entry);
    }
  cerr << "Number of files: " << list.size() << '\n';
  return list;
}

auto convert_to_absolute_path(filesystem::path path) {
  error_code ec;
  return filesystem::absolute(path, filesystem::current_path());
}

enum class canonicality : bool { STRONG, WEAK };
N2W__SPECIALIZE_ENUM(canonicality,
                     N2W__MEMBERS(canonicality::STRONG, canonicality::WEAK));
auto convert_to_canonical_path(filesystem::path path,
                               optional<canonicality> canonical) {
  return filesystem::canonical(path);
}

// auto convert_to_relative_path(filesystem::path path,
//                               optional<filesystem::path> base) {
//   error_code ec;
//   return filesystem::relative(path,
//   base.value_or(filesystem::currentpath()), ec);
// }

// auto convert_to_proximate_path(filesystem::path path,
//                                optional<filesystem::path> base) {
//   error_code ec;
//   return filesystem::proximate(path,
//   base.value_or(filesystem::currentpath()), ec);
// }

plugin plugin = []() {
  n2w::plugin plugin;
  plugin.register_service(DECLARE_API(current_working_directory), "");
  plugin.register_service(DECLARE_API(set_current_working_directory), "");
  plugin.register_service(DECLARE_API(list_files), "");
  plugin.register_service(DECLARE_API(convert_to_absolute_path), "");
  plugin.register_service(DECLARE_API(convert_to_canonical_path), "");
  // plugin.register_service(DECLARE_API(convert_to_relative_path), "");
  // plugin.register_service(DECLARE_API(convert_to_proximate_path), "");
  return plugin;
}();