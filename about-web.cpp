// clang-format off
/*
time g++-5 -g -static -std=c++14 -O3 -pthread -Wall -Werror -Wpedantic -I . -L . -o about-web.gcc -D_GLIBCXX_USE_CXX11_ABI=1 about-web.cpp -lstdc++fs -ldl -lsqlite -lPocoNet -lPocoFoundation -lPocoUtil
 *
 * GUI elements named after action, not representation:
 *
 * Push ~ button
 * Tap ~ check, radio
 * Flip ~ tab, page
 * Turn ~ dial, knob
 * Pull
 * Slide
 * Roll
 * Scatter
 * Grab
 * Drop
 * Read
 * Write
 */
// clang-format on

#include <unordered_map>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <regex>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <experimental/filesystem>

#include "Poco/ThreadPool.h"
#include "Poco/Net/Net.h"
#include "Poco/Net/HTTPServer.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/WebSocket.h"

#include "sqlite3.h"

#include "about-web-rpc.h"
#include "about-web-lpc.h"

static auto mime_types = []() {
  std::unordered_map<std::string, std::string> types;
  std::string line;
  for (std::ifstream file{"data/mime.txt"}; std::getline(file, line);)
  {
    if (!line.empty() && line[0] == '#')
      continue;
    std::string mime;
    std::string extension;
    std::istringstream record{line};
    record >> mime;
    for (; record >> extension;)
    {
      types['.' + extension] = mime;
      std::transform(begin(extension), end(extension), begin(extension),
                     toupper);
      types['.' + extension] = mime;
    }
  }

  return types;
}();

bool shutdown_request = false;
std::mutex shutdown_m;
std::condition_variable shutdown_cv;

namespace fs = std::experimental::filesystem;

std::tuple<
    std::vector<float>, short,
    std::vector<std::tuple<std::string,
                           std::vector<std::vector<std::vector<double>>>, int>>,
    std::tuple<std::vector<double>, std::tuple<int, std::string, float>, float,
               std::string, std::vector<unsigned int>>>
dummy_func(
    const std::vector<float> &, short,
    const std::vector<std::tuple<
        std::string, std::vector<std::vector<std::vector<double>>>, int>> &,
    const std::tuple<std::vector<double>, std::tuple<int, std::string, float>,
                     float, std::string, std::vector<unsigned int>> &)
{
  return {};
}

void void_dummy_func(const std::string &, const std::vector<char> &) {}
bool bool_dummy_func(const std::string &, bool) { return false; }

template <typename T>
auto serialize(const T &t)
    -> std::enable_if_t<std::is_arithmetic<T>::value, T>
{
  return t;
}

template <typename... T>
auto serialize(const T &... t);

template <typename T, typename... Ts>
auto serialize(const std::basic_string<T, Ts...> &t)
{
  return t;
}

auto serialize(const fs::path &p)
{
  return std::accumulate(std::begin(p), std::end(p), std::vector<std::string>{},
                         [](auto &v, const auto &p) {
                           v.push_back(p.u8string());
                           return v;
                         });
}

auto deserialize(const std::vector<std::string> &b, fs::path &&p)
{
  return std::accumulate(begin(b), end(b), p, std::divides<>{});
}

template <typename... Ts>
auto serialize(const std::tuple<Ts...> &ts);

template <typename T, typename... Ts>
auto serialize(const std::vector<T, Ts...> &v)
{
  return std::accumulate(begin(v), end(v),
                         std::vector<decltype(serialize(std::declval<T>()))>{},
                         [](auto &s, const auto &t) {
                           s.push_back(serialize(t));
                           return s;
                         });
}

template <typename T, typename... Ts>
auto serialize(const std::initializer_list<T> &v)
{
  return std::accumulate(begin(v), end(v),
                         std::vector<decltype(serialize(std::declval<T>()))>{},
                         [](auto &s, const auto &t) {
                           s.push_back(serialize(t));
                           return s;
                         });
}

template <std::size_t... I, typename... Ts>
auto serialize(std::integer_sequence<std::size_t, I...>,
               const std::tuple<Ts...> &ts)
{
  return serialize(std::get<I>(ts)...);
}

template <typename... Ts>
auto serialize(const std::tuple<Ts...> &ts)
{
  return serialize(std::make_index_sequence<sizeof...(Ts)>{}, ts);
}

auto home_directory() { return serialize(fs::path{std::getenv("HOME")}); }

const std::string file_type2string[] = {
    "not found", "none", "regular", "directory", "symlink",
    "block", "character", "fifo", "socket", "unknown"};

auto serialize(const std::chrono::system_clock::time_point &tp)
{
  auto time = std::chrono::system_clock::to_time_t(tp);
  std::ostringstream ss;
  ss << std::put_time(std::gmtime(&time), "%FT%T");
  return ss.str();
}

auto serialize(const fs::file_type &type)
{
  return file_type2string[static_cast<unsigned>(type) + 1];
}

auto serialize(const fs::perms &permissions)
{
  std::ostringstream ss;
  ss << std::oct << static_cast<std::size_t>(permissions);
  return ss.str();
}

template <std::size_t... I, typename... T>
auto serialize(std::integer_sequence<std::size_t, I...>, const T &... t)
{
  return std::make_tuple(serialize(t)...);
}

template <typename... T>
auto serialize(const T &... t)
{
  return serialize(std::make_index_sequence<sizeof...(T)>{}, t...);
}

auto file_info(const fs::path &p, bool file_count_as_dir_size,
               std::error_code ec)
{
  auto status = fs::symlink_status(p, ec);
  return serialize(p.filename().u8string(),
                   status.type() == fs::file_type::directory &&
                           file_count_as_dir_size
                       ? std::distance(fs::directory_iterator{p}, {})
                       : fs::file_size(p, ec),
                   status.type(), status.type() == fs::file_type::directory
                                      ? std::string{}
                                      : mime_types[p.extension().u8string()],
                   status.permissions(), fs::last_write_time(p, ec));
}

auto list_files(const std::vector<std::string> &segments,
                bool file_count_as_dir_size)
{
  auto path = deserialize(segments, fs::path{});
  std::cerr << "Requested path: " << path.u8string() << std::endl;
  auto parent_path = path;
  if (!fs::is_directory(parent_path))
    parent_path = parent_path.parent_path();
  std::error_code ec;

  auto info = [file_count_as_dir_size, &ec](const auto &p) {
    std::cerr << "Reading file info: " << p.u8string() << std::endl;
    return file_info(p, file_count_as_dir_size, ec);
  };
  using file_info_type = decltype(info(std::declval<fs::path>()));
  if (fs::is_symlink(path))
  {
    std::cerr << "Reading symlink" << std::endl;
    auto target =
        fs::canonical(fs::absolute(fs::read_symlink(path), parent_path), ec);
    std::cerr << "Symlink: " << path << std::endl;
    std::cerr << "Symlink target: " << target << std::endl;
    path = target;
    parent_path = target.parent_path();
  }

  std::cerr << "Getting space information" << std::endl;
  auto space = fs::space(parent_path, ec);
  if (fs::is_regular_file(path))
  {
    return serialize(
        serialize(parent_path, space.free, space.available, space.capacity),
        std::initializer_list<file_info_type>{info(path)});
  }
  if (fs::is_directory(path))
  {
    std::cerr << "Iterating directory" << std::endl;
    return serialize(
        serialize(parent_path, space.free, space.available, space.capacity),
        std::accumulate(fs::directory_iterator{path}, {},
                        std::vector<file_info_type>{},
                        [&info](auto &v, auto &p) {
                          v.emplace_back(info(p.path()));
                          return v;
                        }));
  }
  return decltype(serialize(
      serialize(parent_path, space.free, space.available, space.capacity),
      std::initializer_list<file_info_type>{info(path)})){};
}

struct path_hash
{
  auto operator()(const fs::path &p) const
  {
    return std::hash<std::string>{}(p.u8string());
  }
};

auto list_roots()
{
  fs::path mtab{"/etc/mtab"};
  if (!fs::exists(mtab))
    return std::vector<std::vector<std::string>>{};

  const std::regex regex{R"(\\\d\d\d|\\[^\d])"};
  static constexpr int submatches[] = {-1, 0};
  const std::regex oct_rx{R"(\\(\d\d\d))"};
  const std::regex esc_rx{R"(\\([^\d]))"};
  std::unordered_map<fs::path, fs::path, path_hash> roots;
  for (std::ifstream mounted{mtab.u8string()}; mounted;
       mounted.ignore(std::numeric_limits<std::streamsize>::max(), '\n'))
  {
    std::string dev;
    std::string mount;
    mounted >> dev >> mount;
    std::cerr << "Looking at: " << dev << ' ' << mount << std::endl;
    fs::path path{dev};
    if (!fs::exists(path) || !fs::is_block_file(path))
      continue;
    mount = std::accumulate(
        std::sregex_token_iterator(begin(mount), end(mount), regex, submatches),
        {}, std::string{}, [&oct_rx, &esc_rx](auto &v, const auto &s) {
          std::smatch match;
          auto e = s.str();
          if (std::regex_match(e, match, oct_rx)) {
            std::istringstream octal{match[1]};
            unsigned value;
            octal >> std::oct >> value;
            v.push_back(value);
          } else if (std::regex_match(e, match, esc_rx)) {
            v += match[1];
          } else
            v += e;
          return v; });
    fs::path root{mount};
    if (roots[path].empty() || root < roots[path])
      roots[path] = root;
  }

  return serialize(std::accumulate(begin(roots), end(roots),
                                   std::vector<fs::path>{},
                                   [](auto &v, const auto &p) {
                                     v.push_back(p.second);
                                     return v;
                                   }));
}

DEFINE_RPC_REGISTER(funcs)
REGISTER_RPC(dummy_func)
,
    REGISTER_RPC(void_dummy_func),
    REGISTER_RPC(bool_dummy_func),
    REGISTER_RPC(home_directory),
    REGISTER_RPC(list_files),
    REGISTER_RPC(list_roots),
    FINALIZE_RPC_REGISTER

#include "about-web-service-handlers.h"

#include "about-web-sqlite.h"

    // TODO: Marshal data types into numbers, strings, vectors and tuples
    // TODO: Settings db
    // TODO: File metadata db
    // TODO: Reactively update dbs
    // TODO: Websocket progress

    int main(int c, char **v)
{
  sql_error(sqlite_initialized, "Initialize SQLite");
  if (sqlite_shutdown_registered != 0)
    std::cerr << "SQLite shutdown wasn't atexit registered" << std::endl;

  SQLite roots_table{"data/fs.db"};

  std::cerr << "Creating module" << std::endl;
  auto rc = sqlite3_create_module(roots_table.db, "fsmodule",
                                  &FilesystemModule::module, nullptr);
  sql_error(rc, "Creating virtual filesystem module");
  std::cerr << "Creating virtual FSROOT table" << std::endl;
  begin(roots_table(
      "CREATE VIRTUAL TABLE IF NOT EXISTS FSROOTS USING fsmodule;"));
  //   sql_error(rc, "Creating filesystem virtual table");
  std::cerr << "Querying FSROOT table" << std::endl;
  for (auto &&row : roots_table("SELECT * FROM FSROOTS"))
  {
    std::string root = "no root", device = "no device";
    std::int64_t capacity, free, available;
    row(root, device, capacity, free, available);
    std::cerr << root << " -> " << device << " (" << capacity << ',' << free
              << ',' << available << ')' << std::endl;
  }

  for (auto &&tie :
       roots_table("SELECT SUM(FREE), SUM(CAPACITY), SUM(AVAILABLE) FROM "
                   "(SELECT FREE, CAPACITY, AVAILABLE FROM FSROOTS)"))
  {
    std::int64_t free, capacity, available;
    tie(free, capacity, available);
    std::cerr << "Total size of disks: " << free << ',' << capacity << ','
              << available << std::endl;
  }

  std::cerr << "Creating virtual FSHOME table" << std::endl;
  begin(roots_table("DROP TABLE FSHOME"));
  begin(roots_table(
#include "scripts/create-virtual-table.sql"
      ));
  std::cerr << "Creating FSHOME cache table" << std::endl;
  begin(roots_table(
#include "scripts/create-fsdump-cached.sql"
      ));
  begin(roots_table(
#include "scripts/create-fsparents-cached.sql"
      ));
  begin(roots_table(
#include "scripts/create-fsfiles-cached.sql"
      ));
  begin(roots_table(
#include "scripts/create-fsstats-cached.sql"
      ));
  begin(roots_table(
#include "scripts/create-concordance.sql"
      ));
  begin(roots_table(
#include "scripts/create-fshome-cached.sql"
      ));
  begin(roots_table(
#include "scripts/create-do-cache.sql"
      ));
  std::cerr << "Insert/replacing rows in cache" << std::endl;

  begin(roots_table("BEGIN TRANSACTION"));

  auto then = std::chrono::system_clock::now();
  begin(roots_table(
#include "scripts/insert-fsdump-cached.sql"
      ));

  begin(roots_table(
#include "scripts/create-xref.sql"
      ));

  begin(roots_table("DELETE FROM CONCORDANCE"));
  begin(roots_table("DROP TABLE CONCORDANCE"));

  auto now = std::chrono::system_clock::now();
  std::cerr << "Time to insert/replace: "
            << std::chrono::duration_cast<std::chrono::seconds>(now - then)
                   .count()
            << std::endl;

  begin(roots_table("COMMIT TRANSACTION"));

  begin(roots_table("DROP TABLE FSDUMP_CACHED"));

  std::ofstream fshome_stream{"data/fshome.txt", std::fstream::trunc};
  then = std::chrono::system_clock::now();
  for (auto &&tie : roots_table("SELECT * FROM FSHOME_CACHED"))
  {
    std::string path, mod_date, file_type, mime_type, parent, stem, extension;
    std::int64_t size = 0;
    int permissions = 0;
    tie(parent, stem, extension, size, mod_date, file_type, mime_type,
        permissions);
    fshome_stream << parent << ',' << stem << ',' << extension << ',' << size
                  << ',' << mod_date << ',' << file_type << ',' << mime_type
                  << ',' << permissions << std::endl;
  }
  std::ofstream fsfiles_stream{"data/fsfiles.txt", std::fstream::trunc};
  for (auto &&tie : roots_table("SELECT * FROM FSFILES_CACHED"))
  {
    std::string stem, extension;
    std::int64_t id = -1, parent_id = -1;
    tie(id, parent_id, stem, extension);
    fsfiles_stream << id << ": " << parent_id << ',' << stem << extension
                   << std::endl;
  }
  std::ofstream fsstats_stream{"data/fsstats.txt", std::fstream::trunc};
  for (auto &&tie : roots_table("SELECT * FROM FSSTATS_CACHED"))
  {
    std::string mod_date, file_type, mime_type;
    std::int64_t size = 0, id = -1;
    int permissions = 0;
    tie(id, size, mod_date, file_type, mime_type, permissions);
    fsstats_stream << id << ": " << size << ',' << mod_date << ',' << file_type
                   << ',' << mime_type << ',' << permissions << std::endl;
  }
  std::ofstream fsparents_stream{"data/fsparents.txt", std::fstream::trunc};
  for (auto &&tie : roots_table("SELECT * FROM FSPARENTS_CACHED"))
  {
    std::string parent;
    std::int64_t id = -1;
    tie(id, parent);
    fsparents_stream << id << ": " << parent << std::endl;
  }
  now = std::chrono::system_clock::now();
  std::cerr << "Time to query: "
            << std::chrono::duration_cast<std::chrono::seconds>(now - then)
                   .count()
            << std::endl;
  for (auto &&tie : roots_table("SELECT COUNT(STEM) FROM FSHOME_CACHED"))
  {
    std::int64_t count;
    tie(count);
    std::cerr << "Number of files: " << count << std::endl;
  }
  for (auto &&tie : roots_table("SELECT COUNT(STEM) FROM FSFILES_CACHED"))
  {
    std::int64_t count;
    tie(count);
    std::cerr << "Number of files: " << count << std::endl;
  }

  for (auto &&tie :
       roots_table("SELECT COUNT(STEM), SUM(FILESIZE), AVG(FILESIZE), "
                   "MAX(FILESIZE) FROM FSHOME_CACHED WHERE "
                   "MIMETYPE LIKE 'video/%'"))
  {
    std::int64_t count, size, avg, max;
    tie(count, size, avg, max);
    std::cerr << "Number of video files: " << count << std::endl;
    std::cerr << "Total size video of files: " << size << std::endl;
    std::cerr << "Average size video of files: " << avg << std::endl;
    std::cerr << "Maximum size video of files: " << max << std::endl;
  }
  for (auto &&tie :
       roots_table("SELECT COUNT(STEM), SUM(FILESIZE), AVG(FILESIZE), "
                   "MAX(FILESIZE) FROM FSHOME_CACHED WHERE "
                   "MIMETYPE LIKE 'audio/%'"))
  {
    std::int64_t count, size, avg, max;
    tie(count, size, avg, max);
    std::cerr << "Number of audio files: " << count << std::endl;
    std::cerr << "Total size audio of files: " << size << std::endl;
    std::cerr << "Average size audio of files: " << avg << std::endl;
    std::cerr << "Maximum size audio of files: " << max << std::endl;
  }
  for (auto &&tie :
       roots_table("SELECT COUNT(STEM), SUM(FILESIZE), AVG(FILESIZE), "
                   "MAX(FILESIZE) FROM FSHOME_CACHED WHERE "
                   "MIMETYPE LIKE 'image/%'"))
  {
    std::int64_t count, size, avg, max;
    tie(count, size, avg, max);
    std::cerr << "Number of image files: " << count << std::endl;
    std::cerr << "Total size image of files: " << size << std::endl;
    std::cerr << "Average size image of files: " << avg << std::endl;
    std::cerr << "Maximum size image of files: " << max << std::endl;
  }
  for (auto &&tie :
       roots_table("SELECT COUNT(STEM), SUM(FILESIZE), AVG(FILESIZE), "
                   "MAX(FILESIZE) FROM FSHOME_CACHED WHERE "
                   "MIMETYPE LIKE 'text/%'"))
  {
    std::int64_t count, size, avg, max;
    tie(count, size, avg, max);
    std::cerr << "Number of text files: " << count << std::endl;
    std::cerr << "Total size text of files: " << size << std::endl;
    std::cerr << "Average size text of files: " << avg << std::endl;
    std::cerr << "Maximum size text of files: " << max << std::endl;
  }
  for (auto &&tie :
       roots_table("SELECT COUNT(STEM), SUM(FILESIZE), AVG(FILESIZE), "
                   "MAX(FILESIZE) FROM FSHOME_CACHED WHERE "
                   "MIMETYPE LIKE 'application/%'"))
  {
    std::int64_t count, size, avg, max;
    tie(count, size, avg, max);
    std::cerr << "Number of application specific files: " << count << std::endl;
    std::cerr << "Total size application of files: " << size << std::endl;
    std::cerr << "Average size application of files: " << avg << std::endl;
    std::cerr << "Maximum size application of files: " << max << std::endl;
  }

  for (auto &&tie : roots_table("SELECT COUNT(STEM), SUM(FILESIZE), "
                                "AVG(FILESIZE), MAX(FILESIZE) FROM "
                                "(SELECT STEM, FILESIZE FROM FSHOME_CACHED "
                                "WHERE FILETYPE = 'regular')"))
  {
    std::int64_t count, size, avg, max;
    tie(count, size, avg, max);
    std::cerr << "Number of files: " << count << std::endl;
    std::cerr << "Total size of files: " << size << std::endl;
    std::cerr << "Average size of files: " << avg << std::endl;
    std::cerr << "Maximum size of files: " << max << std::endl;
  }

  std::ofstream readers{"scripts/readers.js"};
  WRITE_API(readers, dummy_func);
  WRITE_API(readers, void_dummy_func);
  WRITE_API(readers, bool_dummy_func);
  WRITE_API(readers, home_directory);
  WRITE_API(readers, list_files);
  WRITE_API(readers, list_roots);
  readers << std::endl
          << std::endl;

  Poco::Net::HTTPServer server{new AboutWebRequestHandlerFactory, 9090,
                               new Poco::Net::HTTPServerParams};

  server.start();

  std::system("firefox -new-window localhost:9090/index.html");

  {
    std::unique_lock<std::mutex> lock{shutdown_m};
    shutdown_cv.wait(lock, []() { return shutdown_request; });
  }

  server.stopAll();

  return 0;
}
