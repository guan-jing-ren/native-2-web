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
// - Column order, sort order
// - Directory size if directory
// - Filter names and properties
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

plugin plugin = []() {
  n2w::plugin plugin;
  plugin.register_service(DECLARE_API(current_working_directory), "");
  plugin.register_service(DECLARE_API(set_current_working_directory), "");
  plugin.register_service(DECLARE_API(list_files), "");
  return plugin;
}();