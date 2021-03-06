#include "native-2-web-plugin.hpp"

#include <iostream>

using namespace std;
using namespace experimental;
using namespace n2w;

auto current_working_directory() {
  error_code ec;
  return filesystem::current_path(ec);
}

auto set_current_working_directory(const filesystem::path &path) {
  cerr << "Setting current path: " << path << '\n';
  error_code ec;
  filesystem::current_path(path, ec);
  return ec.message();
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
  error_code ec;
  vector<filesystem::directory_entry> list;
  if (paths)
    for (auto &&path : *paths)
      for (auto &&entry : filesystem::directory_iterator{path, ec})
        list.push_back(entry);
  else
    for (auto &&entry :
         filesystem::directory_iterator{current_working_directory(), ec}) {
      list.push_back(entry);
    }
  cerr << "Number of files: " << list.size() << '\n';
  return list;
}

auto convert_to_absolute_path(filesystem::path path) {
  error_code ec;
  return filesystem::absolute(path, filesystem::current_path(ec));
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

auto copy_entity(filesystem::path from, filesystem::path to,
                 optional<vector<filesystem::copy_options>> options) {
  error_code ec;
  auto option = filesystem::copy_options::none;
  if (options)
    option = accumulate(cbegin(*options), cend(*options), option, bit_or<>{});
  filesystem::copy(from, to, option, ec);
  return ec.message();
}

enum class recursivity : bool { RECURSIVE, NOT_RECURSIVE };
N2W__SPECIALIZE_ENUM(recursivity, N2W__MEMBERS(recursivity::RECURSIVE,
                                               recursivity::NOT_RECURSIVE));
auto create_directory(
    filesystem::path path,
    optional<variant<filesystem::path, recursivity>> existing_or_recursive) {
  error_code ec;
  if (existing_or_recursive)
    switch (existing_or_recursive->index()) {
    case 0:
      return filesystem::create_directory(
          path, std::get<0>(*existing_or_recursive), ec);
    case 1:
      if (std::get<1>(*existing_or_recursive) != recursivity::RECURSIVE)
        return filesystem::create_directory(path, ec);
    }
  return filesystem::create_directories(path, ec);
}

auto create_hard_link(filesystem::path target, filesystem::path link) {
  error_code ec;
  filesystem::create_hard_link(target, link, ec);
  return ec.message();
}

enum class target_type { DIRECTORY, FILE };
N2W__SPECIALIZE_ENUM(target_type,
                     N2W__MEMBERS(target_type::DIRECTORY, target_type::FILE));
auto create_symbolic_link(filesystem::path target, filesystem::path link,
                          optional<target_type> type) {
  error_code ec;
  if (type && *type == target_type::DIRECTORY)
    filesystem::create_directory_symlink(target, link, ec);
  else
    filesystem::create_symlink(target, link, ec);
  return ec.message();
}

auto path_exists(
    variant<filesystem::path, filesystem::file_status> path_or_status) {
  error_code ec;
  switch (path_or_status.index()) {
  case 0:
    return filesystem::exists(std::get<0>(path_or_status), ec);
  case 1:
    return filesystem::exists(std::get<1>(path_or_status));
  }
  return false;
}

auto paths_equivalent(filesystem::path l, filesystem::path r) {
  error_code ec;
  return filesystem::equivalent(l, r, ec);
}

auto file_size(filesystem::path path) {
  error_code ec;
  return static_cast<uint32_t>(filesystem::file_size(path, ec));
}

auto hard_link_count(filesystem::path path) {
  error_code ec;
  return static_cast<uint32_t>(filesystem::hard_link_count(path, ec));
}

auto last_write_time(filesystem::path path) {
  error_code ec;
  return filesystem::last_write_time(path, ec);
}

auto set_last_write_time(filesystem::path path,
                         filesystem::file_time_type time) {
  error_code ec;
  filesystem::last_write_time(path, time, ec);
  return ec.message();
}

auto set_permissions(filesystem::path path, filesystem::perms permissions) {
  error_code ec;
  filesystem::permissions(path, permissions, ec);
  return ec.message();
}

auto get_symbolic_link_target(filesystem::path path) {
  error_code ec;
  return filesystem::read_symlink(path, ec);
}

uint32_t remove_path(filesystem::path path, optional<recursivity> recursive) {
  error_code ec;
  auto recurse = recursive.value_or(recursivity::NOT_RECURSIVE);
  switch (recurse) {
  case recursivity::NOT_RECURSIVE:
    return filesystem::remove(path, ec);
  case recursivity::RECURSIVE:
    return filesystem::remove_all(path, ec);
  }
}

auto rename_path(filesystem::path from, filesystem::path to) {
  error_code ec;
  filesystem::rename(from, to, ec);
  return ec.message();
}

auto resize_file(filesystem::path file, uint32_t size) {
  error_code ec;
  filesystem::resize_file(file, size, ec);
  return ec.message();
}

auto space_information(filesystem::path path) {
  error_code ec;
  return filesystem::space(path, ec);
}

enum class symlink_action { NO_FOLLOW, FOLLOW };
N2W__SPECIALIZE_ENUM(symlink_action, N2W__MEMBERS(symlink_action::NO_FOLLOW,
                                                  symlink_action::FOLLOW));
auto path_status(filesystem::path path, optional<symlink_action> action) {
  error_code ec;
  auto act = action.value_or(symlink_action::NO_FOLLOW);
  switch (act) {
  case symlink_action::NO_FOLLOW:
    return filesystem::symlink_status(path, ec);
  case symlink_action::FOLLOW:
    return filesystem::status(path, ec);
  }
}

auto temporary_directory_location() {
  error_code ec;
  return filesystem::temp_directory_path(ec);
}

plugin plugin = []() {
  n2w::plugin plugin;
  plugin.register_service(N2W__DECLARE_API(current_working_directory), "");
  plugin.register_service(N2W__DECLARE_API(set_current_working_directory), "");
  plugin.register_service(N2W__DECLARE_API(list_files), "");
  plugin.register_service(N2W__DECLARE_API(convert_to_absolute_path), "");
  plugin.register_service(N2W__DECLARE_API(convert_to_canonical_path), "");
  // plugin.register_service(N2W__DECLARE_API(convert_to_relative_path), "");
  // plugin.register_service(N2W__DECLARE_API(convert_to_proximate_path), "");
  plugin.register_service(N2W__DECLARE_API(copy_entity), "");
  plugin.register_service(N2W__DECLARE_API(create_directory), "");
  plugin.register_service(N2W__DECLARE_API(create_hard_link), "");
  plugin.register_service(N2W__DECLARE_API(create_symbolic_link), "");
  plugin.register_service(N2W__DECLARE_API(path_exists), "");
  plugin.register_service(N2W__DECLARE_API(paths_equivalent), "");
  plugin.register_service(N2W__DECLARE_API(file_size), "");
  plugin.register_service(N2W__DECLARE_API(hard_link_count), "");
  plugin.register_service(N2W__DECLARE_API(last_write_time), "");
  plugin.register_service(N2W__DECLARE_API(set_last_write_time), "");
  plugin.register_service(N2W__DECLARE_API(set_permissions), "");
  plugin.register_service(N2W__DECLARE_API(get_symbolic_link_target), "");
  plugin.register_service(N2W__DECLARE_API(remove_path), "");
  plugin.register_service(N2W__DECLARE_API(rename_path), "");
  plugin.register_service(N2W__DECLARE_API(resize_file), "");
  plugin.register_service(N2W__DECLARE_API(space_information), "");
  plugin.register_service(N2W__DECLARE_API(path_status), "");
  plugin.register_service(N2W__DECLARE_API(temporary_directory_location), "");
  return plugin;
}();