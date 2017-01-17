#include "native-2-web-manglespec.hpp"
#include "native-2-web-deserialize.hpp"
#include "native-2-web-serialize.hpp"
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

int main(int, char **) {
  std::cout << n2w::endianness<> << '\n';

  std::cout << n2w::mangle<int[90]> << '\n';
  std::cout << n2w::mangle<std::array<std::vector<double>, 5>> << '\n';
  std::cout << n2w::csv<int[90], float, std::u16string> << '\n';
  std::cout
      << n2w::mangle<
             std::pair<int[90], std::array<std::vector<double>, 5>>> << '\n';
  std::cout << n2w::mangle<std::tuple<int[90], float, std::u16string>> << '\n';
  std::cout
      << n2w::mangle<std::multimap<
             std::wstring, std::tuple<std::pair<int, long>, std::vector<double>,
                                      std::array<std::tuple<char16_t, char32_t>,
                                                 15>>>> << '\n';

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

  std::cout << n2w::mangle<void (*)(decltype(a), decltype(b), decltype(c),
                                    decltype(d), decltype(e))> << '\n';
  std::cout << n2w::function_address(swap_test) << '\n';
  std::cout << n2w::function_address(main) << '\n';
  std::uint8_t scrambler[] = {7, 6, 5, 4, 3, 2, 1, 0};
  std::cout << n2w::function_address(swap_test, scrambler) << '\n';
  std::cout << n2w::function_address(main, scrambler) << '\n';

  struct test_structure {
    std::tuple<int[90][81][2]> a;                             // 90*4
    std::array<std::vector<double>, 5> b;                     // 5*4
    std::pair<int[90], std::array<std::vector<double>, 0>> c; // 90*4 + 5*4
    std::tuple<int[90], float, std::unordered_set<std::u16string>>
        d; // 90*4+4+4
    std::multimap<std::wstring,
                  std::tuple<std::pair<int, long[42]>, std::vector<double>,
                             std::array<std::tuple<char16_t, char32_t>, 15>>>
        e; // 4
  } t;

  n2w::structure<test_structure, decltype(test_structure::a),
                 decltype(test_structure::b), decltype(test_structure::c),
                 decltype(test_structure::d), decltype(test_structure::e)>
      test_struct{t,
                  &test_structure::a,
                  &test_structure::b,
                  &test_structure::c,
                  &test_structure::d,
                  &test_structure::e};

  std::cout << n2w::mangle<decltype(test_struct)> << '\n';

  n2w::serialize(test_struct, i);
  n2w::deserialize(j, test_struct);
  std::cout << std::boolalpha << (j == end(ustr)) << ' ' << ustr.size() << ' '
            << std::distance(begin(ustr), j) << '\n';

  return 0;
}