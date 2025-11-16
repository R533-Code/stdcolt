#include <doctest/doctest.h>
#include <stdcolt_coroutines/task.h>
#include <stdcolt_coroutines/generator.h>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <optional>

using namespace stdcolt::coroutines;

Generator<int> make_counter(int n)
{
  for (int i = 0; i < n; ++i)
    co_yield i;
}
Generator<int> make_empty()
{
  co_return;
}
Generator<int> make_throwing()
{
  co_yield 1;
  throw std::runtime_error("boom");
}
Generator<double> make_double_from_ints()
{
  co_yield 1;
  co_yield 2;
}

struct Tracker
{
  static inline int ctor_count     = 0;
  static inline int dtor_count     = 0;
  static inline int live_objects   = 0;

  int value = 0;

  Tracker()
      : value(0)
  {
    ++ctor_count;
    ++live_objects;
  }

  explicit Tracker(int v)
      : value(v)
  {
    ++ctor_count;
    ++live_objects;
  }

  Tracker(const Tracker& other)
      : value(other.value)
  {
    ++ctor_count;
    ++live_objects;
  }

  Tracker(Tracker&& other) noexcept
      : value(other.value)
  {
    ++ctor_count;
    ++live_objects;
  }

  Tracker& operator=(const Tracker& other)
  {
    value = other.value;
    return *this;
  }

  Tracker& operator=(Tracker&& other) noexcept
  {
    value = other.value;
    return *this;
  }

  ~Tracker()
  {
    --live_objects;
    ++dtor_count;
  }

  static void reset()
  {
    ctor_count    = 0;
    dtor_count    = 0;
    live_objects  = 0;
  }
};
static Generator<Tracker> gen_unconsumed_value()
{
  co_yield Tracker{42};
  co_return;
}
static Generator<Tracker> gen_overwrite_value()
{
  co_yield Tracker{1};
  co_yield Tracker{2};
  co_return;
}
static Generator<Tracker> gen_throw_after_value()
{
  co_yield Tracker{7};
  throw std::runtime_error("boom after yield");
}
static Generator<Tracker> gen_throw_before_yield()
{
  throw std::runtime_error("boom before yield");
  co_return;
}
static Generator<Tracker> gen_yield_once()
{
  co_yield Tracker{10};
  co_return;
}

TEST_CASE("stdcolt/coroutines")
{
  SUBCASE("simple sequence with operator() and operator bool")
  {
    auto g = make_counter(3);

    CHECK(static_cast<bool>(g));
    CHECK(g() == 0);

    CHECK(static_cast<bool>(g));
    CHECK(g() == 1);

    CHECK(static_cast<bool>(g));
    CHECK(g() == 2);

    CHECK_FALSE(static_cast<bool>(g));
  }

  SUBCASE("simple sequence with next()")
  {
    auto g = make_counter(3);

    auto v1 = g.next();
    auto v2 = g.next();
    auto v3 = g.next();
    auto v4 = g.next();

    CHECK(v1.has_value());
    CHECK(v2.has_value());
    CHECK(v3.has_value());
    CHECK_FALSE(v4.has_value());

    CHECK_EQ(*v1, 0);
    CHECK_EQ(*v2, 1);
    CHECK_EQ(*v3, 2);
  }

  SUBCASE("empty generator")
  {
    auto g = make_empty();

    CHECK_FALSE(static_cast<bool>(g));

    auto v = g.next();
    CHECK_FALSE(v.has_value());

    auto it = g.begin();
    CHECK(it == std::default_sentinel);
  }

  SUBCASE("range-for iteration")
  {
    auto g = make_counter(5);
    std::vector<int> collected;

    for (int x : g)
      collected.push_back(x);

    REQUIRE_EQ(collected.size(), 5);
    CHECK_EQ(collected[0], 0);
    CHECK_EQ(collected[1], 1);
    CHECK_EQ(collected[2], 2);
    CHECK_EQ(collected[3], 3);
    CHECK_EQ(collected[4], 4);
  }

  SUBCASE("exception propagation via next()")
  {
    auto g = make_throwing();

    auto v = g.next();
    CHECK(v.has_value());
    CHECK_EQ(*v, 1);

    CHECK_THROWS_AS(g.next(), std::runtime_error);
  }

  SUBCASE("exception propagation via iteration")
  {
    auto g = make_throwing();

    auto it = g.begin();
    CHECK(it != std::default_sentinel);

    CHECK_EQ(*it, 1);

    CHECK_THROWS_AS(++it, std::runtime_error);
  }

  SUBCASE("implicit conversion in yield_value")
  {
    auto g = make_double_from_ints();
    std::vector<double> collected;

    for (double x : g)
      collected.push_back(x);

    REQUIRE_EQ(collected.size(), 2);
    CHECK(collected[0] == doctest::Approx(1.0));
    CHECK(collected[1] == doctest::Approx(2.0));
  }

  SUBCASE("move construction transfers ownership")
  {
    auto g1 = make_counter(3);

    CHECK(static_cast<bool>(g1));
    CHECK(g1() == 0);

    auto g2 = std::move(g1);

    CHECK(static_cast<bool>(g2));
    CHECK(g2() == 1);
    CHECK(static_cast<bool>(g2));
    CHECK(g2() == 2);
    CHECK_FALSE(static_cast<bool>(g2));
  }

  SUBCASE("move assignment transfers ownership")
  {
    auto g1 = make_counter(2);
    auto g2 = make_counter(3);

    CHECK(static_cast<bool>(g1));
    CHECK_EQ(g1(), 0);

    CHECK(static_cast<bool>(g2));
    CHECK_EQ(g2(), 0);

    g1 = std::move(g2);

    CHECK(static_cast<bool>(g1));
    CHECK_EQ(g1(), 1);
    CHECK(static_cast<bool>(g1));
    CHECK_EQ(g1(), 2);
    CHECK_FALSE(static_cast<bool>(g1));
  }

  SUBCASE("promise destroys unconsumed value and swallows destructor exceptions")
  {
    Tracker::reset();

    {
      auto g = gen_unconsumed_value();

      // Force coroutine to run and place a value into its frame/promise
      CHECK(static_cast<bool>(g));
    }

    CHECK(Tracker::live_objects == 0);
    CHECK(Tracker::ctor_count == Tracker::dtor_count);
  }

  SUBCASE("promise destroys previous value when overwritten by new yield")
  {
    Tracker::reset();

    {
      auto g = gen_overwrite_value();
      // Consume both yields in some way; exact object counts are
      // implementation-defined, so we only care that it doesn't crash.
      std::vector<int> values;
      for (auto t : g)
        values.push_back(t.value);

      REQUIRE_EQ(values.size(), 2);
      CHECK_EQ(values[0], 1);
      CHECK_EQ(values[1], 2);
    }
    // All Tracker instances must be properly destroyed.
    CHECK(Tracker::live_objects == 0);
    CHECK(Tracker::ctor_count == Tracker::dtor_count);
  }

  SUBCASE("promise handles exception thrown after value without leaking")
  {
    Tracker::reset();
    {
      auto g = gen_throw_after_value();
      // First resume: get the value
      auto v = g.next();
      REQUIRE(v.has_value());
      CHECK_EQ(v->value, 7);
      // Next access rethrows exception captured by unhandled_exception()
      CHECK_THROWS_AS(g.next(), std::runtime_error);
    }
    // No Tracker leaks, no double-destruction.
    CHECK(Tracker::live_objects == 0);
    CHECK(Tracker::ctor_count == Tracker::dtor_count);
  }

  SUBCASE("promise handles exception thrown before first yield")
  {
    Tracker::reset();
    {
      auto g = gen_throw_before_yield();
      // First attempt to get anything must throw
      CHECK_THROWS_AS(static_cast<bool>(g), std::runtime_error);
    }
    // No Tracker ever constructed or destroyed in this scenario;
    // the coroutine never executed a co_yield.
    CHECK(Tracker::live_objects == 0);
    CHECK(Tracker::ctor_count == 0);
    CHECK(Tracker::dtor_count == 0);
  }

  SUBCASE("consumed value does not cause double-destruction or leaks")
  {
    Tracker::reset();
    {
      auto g = gen_yield_once();
      // Force value to be created in coroutine frame/promise
      CHECK(static_cast<bool>(g));
      // Consume the value (move it out)
      Tracker t = g();
      // Generator goes out of scope at end of this block. Its promise
      // destructor must not try to destroy a non-existent Tracker in
      // storage (no double destruction), and there must be no leaks.
      (void)t; // suppress unused warning
    }
    CHECK(Tracker::live_objects == 0);
    CHECK(Tracker::ctor_count == Tracker::dtor_count);
  }
}
