#include "native-2-web-manglespec.hpp"
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

int main() {
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

  swap_test();

  return 0;
}