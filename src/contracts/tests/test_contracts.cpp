#include <doctest/doctest.h>
#include <stdcolt_contracts/contracts.h>

static unsigned violation_counts = 0;

TEST_CASE("stdcolt/contracts")
{
  using namespace stdcolt::contracts;

  // cheat a bit: rather than aborting, count the violations.
  register_violation_handler([](const char*, const char*, Kind,
                                const std::optional<std::source_location>&) noexcept
                             { ++violation_counts; });

  STDCOLT_pre("evaluates to true", "");
  STDCOLT_post(10, "");
  STDCOLT_assert(1.0f, "");
  CHECK(violation_counts == 0);

  STDCOLT_pre(nullptr, "");
  STDCOLT_post(false, "");
  CHECK(violation_counts == 2);
}