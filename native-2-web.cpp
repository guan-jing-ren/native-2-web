#include "native-2-web-manglespec.hpp"
#include <iostream>

int main() {
  std::cout << n2w::mangle<int[90]>::value << '\n';
  std::cout << n2w::mangle<std::array<std::vector<double>, 5>>::value << '\n';
  std::cout << n2w::csv<int[90], float, std::u16string>::value << '\n';
  std::cout
      << n2w::mangle<
             std::pair<int[90], std::array<std::vector<double>, 5>>>::value << '\n';
  auto mangled = n2w::mangle<std::tuple<int>>::value;

  return 0;
}