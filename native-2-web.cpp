#include "native-2-web-readwrite.hpp"
#include "native-2-web-js.hpp"
#include <iostream>
#include <algorithm>

struct test_substructure {
  std::pair<int, std::array<std::vector<double>, 0>> c;         // 90*4 + 5*4
  std::tuple<int, float, std::unordered_set<std::u16string>> d; // 90*4+4+4
};

READ_WRITE_SPEC(test_substructure, (c)(d));
JS_SPEC(test_substructure, (c)(d));

struct test_structure {
  std::tuple<int> a;                    // 90*4
  std::array<std::vector<double>, 5> b; // 5*4
  test_substructure b1;
  std::pair<int, std::array<std::vector<double>, 0>> c;         // 90*4 + 5*4
  std::tuple<int, float, std::unordered_set<std::u16string>> d; // 90*4+4+4
  std::multimap<std::wstring,
                std::tuple<std::pair<int, long>, std::vector<double>,
                           std::array<std::tuple<char16_t, char32_t>, 15>>>
      e; // 4
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
  cout << n2w::to_js<char16_t>::create_reader() << '\n';
  cout << n2w::to_js<char32_t[3][4][5][2]>::create_reader() << '\n';
  cout << n2w::to_js<vector<double>[3][2][4][5]>::create_reader() << '\n';
  cout << n2w::to_js<
              array<array<array<array<float, 3>, 2>, 4>, 5>>::create_reader()
       << '\n';
  cout << n2w::to_js<vector<int>>::create_reader() << '\n';
  cout << n2w::to_js<vector<vector<int>>>::create_reader() << '\n';

  cout << n2w::to_js<list<vector<float>>>::create_reader() << '\n';
  cout << n2w::to_js<forward_list<vector<float>>>::create_reader() << '\n';
  cout << n2w::to_js<deque<vector<float>>>::create_reader() << '\n';
  cout << n2w::to_js<set<vector<float>>>::create_reader() << '\n';
  cout << n2w::to_js<multiset<vector<float>>>::create_reader() << '\n';
  cout << n2w::to_js<unordered_set<vector<float>>>::create_reader() << '\n';
  cout << n2w::to_js<unordered_multiset<vector<float>>>::create_reader()
       << '\n';

  cout << n2w::to_js<
              map<vector<int>,
                  unordered_multimap<int, unordered_set<int>>>>::create_reader()
       << '\n';
  cout << n2w::to_js<multimap<int, vector<int>>>::create_reader() << '\n';
  cout << n2w::to_js<
              unordered_map<unordered_set<int>, std::string>>::create_reader()
       << '\n';
  cout << n2w::to_js<unordered_multimap<int, int>>::create_reader() << '\n';

  cout << n2w::to_js<tuple<int, double, char16_t>>::create_reader() << '\n';
  cout << n2w::to_js<tuple<string>>::create_reader() << "\n\n";

  cout << "// Structure JS test\n";
  cout << "let test_structure_read = "
       << n2w::to_js<test_structure>::create_reader() << '\n';

  vector<uint8_t> data;
  n2w::filler<test_structure> filler;
  filler();
  filler();
  filler();
  filler();
  auto strct = filler();
  n2w::serialize(strct, back_inserter(data));
  cerr << "Pad: " << P << '\n';
  cerr << "Size of structure fill: " << data.size() << '\n';
  n2w::debug_print(cerr, strct) << '\n';

  cout << "let rval = Uint8Array.from(["
       << accumulate(begin(data), end(data), string{},
                     [](const auto &result, const auto &elem) {
                       return result + (result.empty() ? "" : ",") +
                              to_string((unsigned)elem);
                     })
       << "]).buffer;\n";
  cout << "document.write('<pre>' + JSON.stringify(test_structure_read(rval, "
          "0)[0], null, "
          "'\\t') + '</pre>');\n";

  return 0;
}