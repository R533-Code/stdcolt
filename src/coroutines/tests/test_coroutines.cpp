#include <doctest/doctest.h>
#include <iostream>
#include <stdcolt_coroutines/coroutines.h>

TEST_CASE("stdcolt/coroutines")
{
  using namespace stdcolt::coroutines;

  for (auto&& [pair, i] :
       zip(enumerate(map(
               filter(
                   take(drop(iota(10), 10), 10), [](const int& a) { return a % 2; }),
               [](int&& a) { return (float)a / 10.0; })),
           range(-5, -15, -1)))
  {
    auto&& [e, v] = pair;
    std::cout << e << ": " << v << ": " << i << '\n';
  }
}
