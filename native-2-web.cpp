#include "native-2-web-readwrite.hpp"
#include "native-2-web-js.hpp"
#include <iostream>
#include <algorithm>

struct test_substructure {
  std::pair<int, std::array<std::vector<double>, 0>> c;         // 90*4 + 5*4
  std::tuple<int, float, std::unordered_set<std::u16string>> d; // 90*4+4+4
};

READ_WRITE_SPEC(test_substructure, (c)(d));

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

test_structure test_function(test_structure &&t) {
  std::cout << "Test function execution test\n";
  n2w::debug_print(std::cout, t) << '\n';
  return t;
}

int main(int, char **) {
  using namespace std;
  cout << n2w::to_js<char16_t>::create() << '\n';
  cout << n2w::to_js<vector<int>>::create() << '\n';
  cout << n2w::to_js<vector<vector<int>>>::create() << '\n';

  cout << n2w::to_js<list<vector<float>>>::create() << '\n';
  cout << n2w::to_js<forward_list<vector<float>>>::create() << '\n';
  cout << n2w::to_js<deque<vector<float>>>::create() << '\n';
  cout << n2w::to_js<set<vector<float>>>::create() << '\n';
  cout << n2w::to_js<multiset<vector<float>>>::create() << '\n';
  cout << n2w::to_js<unordered_set<vector<float>>>::create() << '\n';
  cout << n2w::to_js<unordered_multiset<vector<float>>>::create() << '\n';

  cout << n2w::to_js<map<vector<int>,
                         unordered_multimap<int, unordered_set<int>>>>::create()
       << '\n';
  cout << n2w::to_js<multimap<int, vector<int>>>::create() << '\n';
  cout << n2w::to_js<unordered_map<unordered_set<int>, int>>::create() << '\n';
  cout << n2w::to_js<unordered_multimap<int, int>>::create() << '\n';

  return 0;
}