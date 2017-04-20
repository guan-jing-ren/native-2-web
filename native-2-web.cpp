#include "native-2-web-js.hpp"
#include "native-2-web-readwrite.hpp"
#include <algorithm>
#include <iostream>

enum class test_enum {
  zero,
  one,
  two,
  three,
  four,
  five,
  six,
  seven,
  eight,
  nine,
  ten
};

SPECIALIZE_ENUM(test_enum, (test_enum::zero)(test_enum::two)(test_enum::five)(
                               test_enum::eight));

struct struct_enum {
  test_enum e = test_enum::five;
};

READ_WRITE_SPEC(struct_enum, (e));
JS_SPEC(struct_enum, (e));

using string_bool = std::tuple<std::string, bool>;
struct test_substructure : public string_bool, public struct_enum {
  std::pair<int, std::array<std::vector<double>, 0>> c;         // 90*4 + 5*4
  std::tuple<int, float, std::unordered_set<std::u16string>> d; // 90*4+4+4
};

READ_WRITE_SPEC(test_substructure, (c)(d), string_bool, struct_enum);
JS_SPEC(test_substructure, (c)(d), string_bool, struct_enum);

struct test_structure {
  std::tuple<int> a;                    // 90*4
  std::array<std::vector<double>, 5> b; // 5*4
  test_substructure b1;
  std::pair<int, std::array<std::vector<double>, 0>> c;         // 90*4 + 5*4
  std::tuple<int, float, std::unordered_set<std::u16string>> d; // 90*4+4+4
  std::multimap<std::wstring,
                std::tuple<std::pair<int, short>, std::vector<double>,
                           std::array<std::tuple<char16_t, char32_t>, 15>>>
      e;                    // 4
  float arrays[3][4][5][2]; // Ignore for now
};

READ_WRITE_SPEC(test_structure, (a)(b)(b1)(c)(d)(e));
JS_SPEC(test_structure, (a)(b)(b1)(c)(d)(e));

test_structure test_function(test_structure &&t) {
  std::cout << "Test function execution test\n";
  n2w::debug_print(std::cout, t) << '\n';
  return t;
}

int main(int, char **) {
  using namespace std;
  // cout << n2w::to_js<char16_t>::create_reader() << '\n';
  // cout << n2w::to_js<char32_t[3][4][5][2]>::create_reader() << '\n';
  // cout << n2w::to_js<vector<double>[3][2][4][5]>::create_reader() << '\n';
  // cout << n2w::to_js<
  //             array<array<array<array<float, 3>, 2>, 4>, 5>>::create_reader()
  //      << '\n';
  // cout << n2w::to_js<vector<int>>::create_reader() << '\n';
  // cout << n2w::to_js<vector<vector<int>>>::create_reader() << '\n';

  // cout << n2w::to_js<list<vector<float>>>::create_reader() << '\n';
  // cout << n2w::to_js<forward_list<vector<float>>>::create_reader() << '\n';
  // cout << n2w::to_js<deque<vector<float>>>::create_reader() << '\n';
  // cout << n2w::to_js<set<vector<float>>>::create_reader() << '\n';
  // cout << n2w::to_js<multiset<vector<float>>>::create_reader() << '\n';
  // cout << n2w::to_js<unordered_set<vector<float>>>::create_reader() << '\n';
  // cout << n2w::to_js<unordered_multiset<vector<float>>>::create_reader()
  //      << '\n';

  // cout << n2w::to_js<
  //             map<vector<int>,
  //                 unordered_multimap<int,
  //                 unordered_set<int>>>>::create_reader()
  //      << '\n';
  // cout << n2w::to_js<multimap<int, vector<int>>>::create_reader() << '\n';
  // cout << n2w::to_js<
  //             unordered_map<unordered_set<int>,
  //             std::string>>::create_reader()
  //      << '\n';
  // cout << n2w::to_js<unordered_multimap<int, int>>::create_reader() << '\n';

  // cout << n2w::to_js<tuple<int, double, char16_t>>::create_reader() << '\n';
  // cout << n2w::to_js<tuple<string>>::create_reader() << "\n\n";

  cout << "// Structure JS test\n";
  cout << "let test_structure_read = "
       << n2w::to_js<test_structure>::create_reader() << '\n';
  cout << "let test_structure_write = "
       << n2w::to_js<test_structure>::create_writer() << '\n';

  vector<uint8_t> data;
  n2w::filler<test_structure> filler;
  filler();
  filler();
  filler();
  filler();
  auto strct = filler();
  n2w::serialize(strct, back_inserter(data));
  cerr << "Size of structure fill: " << data.size() << '\n';
  n2w::debug_print(cerr, strct) << '\n';

  cout << "let rval = new DataView(Uint8Array.from(["
       << accumulate(begin(data), end(data), string{},
                     [](const auto &result, const auto &elem) {
                       return result + (result.empty() ? "" : ",") +
                              to_string((unsigned)elem);
                     })
       << "]).buffer);\n";
  cout << "rval = test_structure_read(rval, 0)[0];\n";
  cout << "d3.select('body').append('pre').text(JSON.stringify(rval, null, "
          "'\\t'));\n";
  cout << "let valr = new DataView(test_structure_write(rval));\n";
  cout << "console.log(valr);\n";
  cout << "let rval2 = test_structure_read(valr, 0)[0];\n";
  cout << "d3.select('body').append('pre').text(JSON.stringify(rval2, null, "
          "'\\t'));\n";
  cout << R"(let test_structure_html = function(parent) {
  let value = {}, dispatcher = d3.dispatch('gather');
  ()" + n2w::to_js<test_structure>::create_html() +
              R"()(parent, v => value = v, dispatcher);
  d3.select(parent)
    .append('input')
    .attr('type', 'button')
    .attr('value','test_structure_html')
    .on('click', () => {
      dispatcher.call('gather');
      d3.select(parent).append('pre').text(JSON.stringify(value, null, '\t'));
      let eulav = test_structure_read(new DataView(test_structure_write(value)), 0)[0];
      d3.select(parent).append('pre').text(JSON.stringify(eulav, null, '\t'));
    });
};
test_structure_html(d3.select('body').node());)";

  return 0;
}