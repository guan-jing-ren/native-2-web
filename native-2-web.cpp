#include "native-2-web-readwrite.hpp"
#include <iostream>
#include <algorithm>

void swap_test() noexcept {
  auto s =
      0b0'0000'0001'0010'0011'0100'0101'0110'0111'1000'1001'1010'1011'1100'1101'1110'1111;
  auto s1 = s;
  std::reverse(reinterpret_cast<char *>(&s1),
               reinterpret_cast<char *>(&s1 + 1));
  auto s2 = reverse_endian(s);
  std::cout << s << ' ' << s2 << ' ' << s1 << '\n';
}

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
  std::cout << n2w::endianness<> << '\n';
  std::cout << reverse_endian(reverse_endian(3.14l)) << '\n';

  std::cout << n2w::mangled<int[90]>() << '\n';
  std::cout << n2w::mangled<std::array<std::vector<double>, 5>>() << '\n';
  std::cout << n2w::csv<int[90], float, std::u16string>() << '\n';
  std::cout
      << n2w::mangled<std::pair<int[90], std::array<std::vector<double>, 5>>>()
      << '\n';
  std::cout << n2w::mangled<std::tuple<int[90], float, std::u16string>>()
            << '\n';
  std::cout
      << n2w::mangled<std::multimap<
             std::wstring,
             std::tuple<std::pair<int, long>, std::vector<double>,
                        std::array<std::tuple<char16_t, char32_t>, 15>>>>()
      << '\n';

  std::vector<char> ustr;
  ustr.reserve(5000000);
  std::tuple<int[90][81][2]> a;                             // 90*4
  std::array<std::vector<double>, 5> b;                     // 5*4
  std::pair<int[90], std::array<std::vector<double>, 0>> c; // 90*4 + 5*4
  std::tuple<int[90], float, std::unordered_set<std::u16string>> d; // 90*4+4+4
  std::multimap<std::wstring,
                std::tuple<std::pair<int, long[42]>, std::vector<double>,
                           std::array<std::tuple<char16_t, char32_t>, 15>>>
      e; // 4

  auto i = std::back_inserter(ustr);
  auto j = begin(ustr);
  n2w::serialize(a, i);
  n2w::deserialize(j, a);
  std::cout << std::boolalpha << (j == end(ustr)) << ' ' << ustr.size() << ' '
            << std::distance(begin(ustr), j) << '\n';

  n2w::serialize(b, i);
  n2w::deserialize(j, b);
  std::cout << std::boolalpha << (j == end(ustr)) << ' ' << ustr.size() << ' '
            << std::distance(begin(ustr), j) << '\n';

  n2w::serialize(c, i);
  n2w::deserialize(j, c);
  std::cout << std::boolalpha << (j == end(ustr)) << ' ' << ustr.size() << ' '
            << std::distance(begin(ustr), j) << '\n';

  n2w::serialize(d, i);
  n2w::deserialize(j, d);
  std::cout << std::boolalpha << (j == end(ustr)) << ' ' << ustr.size() << ' '
            << std::distance(begin(ustr), j) << '\n';

  n2w::serialize(e, i);
  n2w::deserialize(j, e);
  std::cout << std::boolalpha << (j == end(ustr)) << ' ' << ustr.size() << ' '
            << std::distance(begin(ustr), j) << '\n';

  swap_test();

  std::cout << n2w::mangled<void (*)(decltype(a), decltype(b), decltype(c),
                                     decltype(d), decltype(e))>()
            << '\n';
  std::cout << n2w::function_address(swap_test) << '\n';
  std::cout << n2w::function_address(main) << '\n';
  std::uint8_t scrambler[] = {7, 6, 5, 4, 3, 2, 1, 0};
  std::cout << n2w::function_address(swap_test, scrambler) << '\n';
  std::cout << n2w::function_address(main, scrambler) << '\n';

  test_structure t;

  std::cout << n2w::mangle<test_structure> << '\n';

  n2w::serialize(t, i);
  n2w::deserialize(j, t);
  std::cout << std::boolalpha << (j == end(ustr)) << ' ' << ustr.size() << ' '
            << std::distance(begin(ustr), j) << '\n';
  std::cout << sizeof(long double) << '\n';

  ustr.clear();
  i = back_inserter(ustr);
  j = begin(ustr);
  n2w::execute(i, j, swap_test);
  std::cout << std::boolalpha << (j == end(ustr)) << ' ' << ustr.size() << ' '
            << std::distance(begin(ustr), j) << '\n';
  n2w::serialize(t, i);
  std::cout << std::boolalpha << (j == end(ustr)) << ' ' << ustr.size() << ' '
            << std::distance(begin(ustr), j) << '\n';

  bool m[2][3][4];
  n2w::debug_print(std::cout, m) << '\n';
  n2w::debug_print(std::cout, a) << '\n';
  n2w::debug_print(std::cout, b) << '\n';
  n2w::debug_print(std::cout, c) << '\n';
  n2w::debug_print(std::cout, d) << '\n';
  n2w::debug_print(std::cout, e) << '\n';
  n2w::debug_print(std::cout, "Hello world!") << '\n';
  char s[] = "goodbye world";
  n2w::debug_print(std::cout, s) << '\n';

  n2w::execute(i, j, test_function);
  n2w::execute(i, j, test_function);
  std::cout << std::boolalpha << (j == end(ustr)) << ' ' << ustr.size() << ' '
            << std::distance(begin(ustr), j) << '\n';
  n2w::deserialize(j, t);
  std::cout << std::boolalpha << (j == end(ustr)) << ' ' << ustr.size() << ' '
            << std::distance(begin(ustr), j) << '\n';

  test_structure u;
  std::cout << "Equality test: " << (t == u) << '\n';
  std::cout << "Inequality test: " << (t != u) << '\n';

  std::get<0>(u.a) = 1;
  std::cout << "Short compare test: " << (t == u) << '\n';

  std::cout << "Less test: " << (t < u) << '\n';
  std::cout << "Greater test: " << (t > u) << '\n';
  std::cout << "Less test: " << (u < t) << '\n';
  std::cout << "Greater test: " << (u > t) << '\n';
  std::cout << "Less test: " << (t < t) << '\n';
  std::cout << "Greater test: " << (t > t) << '\n';
  std::get<0>(u.a) = 0;
  std::cout << "Less test: " << (t < u) << '\n';
  std::cout << "Less equal test: " << (t <= u) << '\n';
  std::cout << "Greater equal test: " << (t >= u) << '\n';

  return 0;
}