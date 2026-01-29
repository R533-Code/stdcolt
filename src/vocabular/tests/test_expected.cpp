#include <doctest/doctest.h>

#include <string>
#include <utility>
#include <type_traits>
#include <functional>
#include <cstddef>
#include <cstdlib>

#include <stdcolt_vocabular/expected.h>

using namespace stdcolt;

// ---------- helpers to instantiate APIs ----------

static int g_abort_hook_calls = 0;
static void abort_hook() noexcept
{
  ++g_abort_hook_calls;
}

// A move-only type to exercise move paths
struct MoveOnly
{
  int v{};
  MoveOnly() = default;
  explicit MoveOnly(int x)
      : v(x)
  {
  }
  MoveOnly(const MoveOnly&)            = delete;
  MoveOnly& operator=(const MoveOnly&) = delete;
  MoveOnly(MoveOnly&& o) noexcept
      : v(o.v)
  {
    o.v = -1;
  }
  MoveOnly& operator=(MoveOnly&& o) noexcept
  {
    if (this != &o)
    {
      v   = o.v;
      o.v = -1;
    }
    return *this;
  }
};

// A simple aggregate-like value type
struct Pair
{
  int a{};
  int b{};
  Pair() = default;
  Pair(int x, int y)
      : a(x)
      , b(y)
  {
  }
};

static Expected<int, std::string> half_if_even(int x)
{
  if (x % 2 == 0)
    return x / 2;
  return {Unexpected, std::string("odd")};
}

static Expected<int, std::string> plus_one_expected(int x)
{
  return Expected<int, std::string>(x + 1);
}

static Expected<int, std::string> recover_to_7(const std::string& msg)
{
  if (msg == "bad")
    return 7;
  return Expected<int, std::string>(Unexpected, std::string("unhandled"));
}

TEST_CASE("constructors: value, error, in_place, unexpected-tag")
{
  (void)Unexpected;
  (void)InPlace;

  // Default expected value
  Expected<int, std::string> a;
  CHECK(a.is_expect());
  CHECK(a);

  // Copy value
  Expected<int, std::string> b(12);
  CHECK(b.is_expect());
  CHECK(*b == 12);

  // Move value
  int tmp = 13;
  Expected<int, std::string> c(std::move(tmp));
  CHECK(c.is_expect());
  CHECK(c.value() == 13);

  // Default error
  Expected<int, std::string> e0(Unexpected);
  CHECK(e0.is_error());
  CHECK(!e0);

  // Copy error
  std::string msg = "nope";
  Expected<int, std::string> e1(Unexpected, msg);
  CHECK(e1.is_error());
  CHECK(e1.error() == "nope");

  // Move error
  std::string msg2 = "boom";
  Expected<int, std::string> e2(Unexpected, std::move(msg2));
  CHECK(e2.is_error());
  CHECK(e2.error() == "boom");

  // In-place error construction
  Expected<int, std::string> e3(InPlace, Unexpected, 4, 'x'); // "xxxx"
  CHECK(e3.is_error());
  CHECK(e3.error() == "xxxx");

  // In-place expected construction (multi-arg)
  Expected<Pair, std::string> p(InPlace, 1, 2);
  CHECK(p.is_expect());
  CHECK(p->a == 1);
  CHECK((*p).b == 2);

  // In-place expected construction with explicit first arg type (also instantiates constraint path)
  Expected<std::string, std::string> s(InPlace, "hi", 2); // "hi"
  CHECK(s.is_expect());
  CHECK(s.value() == "hi");
}

TEST_CASE("copy/move ctor + copy/move assignment (both value and error states)")
{
  Expected<int, std::string> v1(10);
  Expected<int, std::string> e1(Unexpected, std::string("e"));

  // Copy ctor
  Expected<int, std::string> v2(v1);
  Expected<int, std::string> e2(e1);
  CHECK(v2.is_expect());
  CHECK(*v2 == 10);
  CHECK(e2.is_error());
  CHECK(e2.error() == "e");

  // Move ctor
  Expected<int, std::string> v3(std::move(v2));
  Expected<int, std::string> e3(std::move(e2));
  CHECK(v3.is_expect());
  CHECK(*v3 == 10);
  CHECK(e3.is_error());
  CHECK(e3.error() == "e");

  // Copy assignment
  Expected<int, std::string> x(1);
  x = e1; // value -> error
  CHECK(x.is_error());
  CHECK(x.error() == "e");

  x = v1; // error -> value
  CHECK(x.is_expect());
  CHECK(x.value() == 10);

  // Self copy assignment
  x = x;
  CHECK(x.is_expect());
  CHECK(*x == 10);

  // Move assignment
  Expected<int, std::string> y(Unexpected, std::string("z"));
  y = std::move(v1); // error -> value
  CHECK(y.is_expect());
  CHECK(*y == 10);

  Expected<int, std::string> z(99);
  z = std::move(e1); // value -> error
  CHECK(z.is_error());
  CHECK(z.error() == "e");

  // Self move assignment guard
  z = std::move(z);
  CHECK(z.is_error());
  CHECK(z.error() == "e");
}

TEST_CASE("operator-> and operator* overloads (const/non-const, lvalue/rvalue)")
{
  Expected<Pair, std::string> p(InPlace, 3, 4);

  // operator-> non-const
  CHECK(p->a == 3);
  p->a = 5;
  CHECK(p->a == 5);

  // operator-> const
  const auto& cp = p;
  CHECK(cp->b == 4);

  // operator* lvalue
  (*p).b = 6;
  CHECK((*p).b == 6);

  // operator* const lvalue
  CHECK((*cp).a == 5);

  // operator* rvalue
  Expected<std::string, std::string> s("abc");
  std::string moved = *std::move(s);
  CHECK(moved == "abc");

  // operator* const rvalue
  const Expected<std::string, std::string> cs("def");
  std::string moved2 = *std::move(cs);
  CHECK(moved2 == "def");
}

TEST_CASE("value() overloads (const/non-const, lvalue/rvalue) and error() overloads")
{
  Expected<std::string, std::string> v("abc");
  CHECK(v.value() == "abc");
  v.value().push_back('d');
  CHECK(v.value() == "abcd");

  const Expected<std::string, std::string> cv("xyz");
  CHECK(cv.value() == "xyz");

  Expected<std::string, std::string> mv("move");
  std::string out = std::move(mv).value();
  CHECK(out == "move");

  const Expected<std::string, std::string> cmv("cmove");
  std::string out2 = std::move(cmv).value();
  CHECK(out2 == "cmove");

  Expected<int, std::string> e(Unexpected, std::string("err"));
  CHECK(e.error() == "err");
  e.error().push_back('2');
  CHECK(e.error() == "err2");

  const Expected<int, std::string> ce(Unexpected, std::string("cerr"));
  CHECK(ce.error() == "cerr");

  Expected<int, std::string> me(Unexpected, std::string("merr"));
  std::string em = std::move(me).error();
  CHECK(em == "merr");

  const Expected<int, std::string> cme(Unexpected, std::string("cmerr"));
  std::string em2 = std::move(cme).error();
  CHECK(em2 == "cmerr");
}

TEST_CASE("value_or(U) and value_or(Fn) overloads (const& and &&)")
{
  Expected<int, std::string> ok(5);
  Expected<int, std::string> err(Unexpected, std::string("x"));

  // const& value_or(U)
  const auto& cok  = ok;
  const auto& cerr = err;
  CHECK(cok.value_or(9) == 5);
  CHECK(cerr.value_or(9) == 9);

  // && value_or(U)
  CHECK(Expected<int, std::string>(6).value_or(9) == 6);
  CHECK(Expected<int, std::string>(Unexpected, std::string("x")).value_or(9) == 9);

  // const& value_or(Fn)
  CHECK(cok.value_or([] { return 11; }) == 5);
  CHECK(cerr.value_or([] { return 11; }) == 11);

  // && value_or(Fn)
  CHECK(Expected<int, std::string>(7).value_or([] { return 12; }) == 7);
  CHECK(
      Expected<int, std::string>(Unexpected, std::string("x"))
          .value_or([] { return 12; })
      == 12);
}

TEST_CASE("or_else overloads (&, const&, &&, const&&)")
{
  Expected<int, std::string> ok(2);
  Expected<int, std::string> err(Unexpected, std::string("bad"));

  // &
  auto a = ok.or_else(
      [](std::string& s) -> Expected<int, std::string>
      {
        s += "!"; // should not run
        return 0;
      });
  CHECK(a.is_expect());
  CHECK(*a == 2);

  auto b = err.or_else(
      [](std::string& s) -> Expected<int, std::string>
      {
        CHECK(s == "bad");
        return recover_to_7(s);
      });
  CHECK(b.is_expect());
  CHECK(*b == 7);

  // const&
  const Expected<int, std::string> cok(3);
  const Expected<int, std::string> cerr(Unexpected, std::string("bad"));

  auto c = cok.or_else(
      [](const std::string&) -> Expected<int, std::string> { return 9; });
  CHECK(c.is_expect());
  CHECK(*c == 3);

  auto d = cerr.or_else(
      [](const std::string& s) -> Expected<int, std::string>
      { return recover_to_7(s); });
  CHECK(d.is_expect());
  CHECK(*d == 7);

  // &&
  auto e = Expected<int, std::string>(4).or_else(
      [](std::string&&) -> Expected<int, std::string> { return 0; });
  CHECK(e.is_expect());
  CHECK(*e == 4);

  auto f = Expected<int, std::string>(Unexpected, std::string("bad"))
               .or_else(
                   [](std::string&& s) -> Expected<int, std::string>
                   { return recover_to_7(s); });
  CHECK(f.is_expect());
  CHECK(*f == 7);

  // const&&
  const Expected<int, std::string> cok2(5);
  const Expected<int, std::string> cerr2(Unexpected, std::string("bad"));

  auto g = std::move(cok2).or_else(
      [](const std::string&&) -> Expected<int, std::string> { return 0; });
  CHECK(g.is_expect());
  CHECK(*g == 5);

  auto h = std::move(cerr2).or_else(
      [](const std::string&& s) -> Expected<int, std::string>
      { return recover_to_7(s); });
  CHECK(h.is_expect());
  CHECK(*h == 7);
}

TEST_CASE("and_then overloads (&, const&, &&, const&&)")
{
  Expected<int, std::string> ok(8);
  Expected<int, std::string> err(Unexpected, std::string("boom"));

  // &
  auto a = ok.and_then(half_if_even);
  CHECK(a.is_expect());
  CHECK(*a == 4);

  auto b = err.and_then(half_if_even);
  CHECK(b.is_error());
  CHECK(b.error() == "boom");

  // const&
  const Expected<int, std::string> cok(10);
  const Expected<int, std::string> cerr(Unexpected, std::string("boom"));
  auto c = cok.and_then(half_if_even);
  CHECK(c.is_expect());
  CHECK(*c == 5);

  auto d = cerr.and_then(half_if_even);
  CHECK(d.is_error());
  CHECK(d.error() == "boom");

  // &&
  auto e = Expected<int, std::string>(12).and_then(half_if_even);
  CHECK(e.is_expect());
  CHECK(*e == 6);

  auto f = Expected<int, std::string>(Unexpected, std::string("boom"))
               .and_then(half_if_even);
  CHECK(f.is_error());
  CHECK(f.error() == "boom");

  // const&&
  const Expected<int, std::string> cok2(14);
  const Expected<int, std::string> cerr2(Unexpected, std::string("boom"));
  auto g = std::move(cok2).and_then(half_if_even);
  CHECK(g.is_expect());
  CHECK(*g == 7);

  auto h = std::move(cerr2).and_then(half_if_even);
  CHECK(h.is_error());
  CHECK(h.error() == "boom");
}

TEST_CASE("map overloads (&, const&, &&, const&&)")
{
  Expected<int, std::string> ok(3);
  Expected<int, std::string> err(Unexpected, std::string("no"));

  // &
  auto a = ok.map([](int x) { return x * 10; });
  CHECK(a.is_expect());
  CHECK(*a == 30);

  auto b = err.map([](int x) { return x * 10; });
  CHECK(b.is_error());
  CHECK(b.error() == "no");

  // const&
  const Expected<int, std::string> cok(4);
  const Expected<int, std::string> cerr(Unexpected, std::string("no"));
  auto c = cok.map([](int x) { return x + 1; });
  CHECK(c.is_expect());
  CHECK(*c == 5);

  auto d = cerr.map([](int x) { return x + 1; });
  CHECK(d.is_error());
  CHECK(d.error() == "no");

  // &&
  auto e = Expected<int, std::string>(5).map([](int x) { return x - 2; });
  CHECK(e.is_expect());
  CHECK(*e == 3);

  auto f = Expected<int, std::string>(Unexpected, std::string("no"))
               .map([](int x) { return x - 2; });
  CHECK(f.is_error());
  CHECK(f.error() == "no");

  // const&&
  const Expected<int, std::string> cok2(6);
  const Expected<int, std::string> cerr2(Unexpected, std::string("no"));
  auto g = std::move(cok2).map([](int x) { return x * x; });
  CHECK(g.is_expect());
  CHECK(*g == 36);

  auto h = std::move(cerr2).map([](int x) { return x * x; });
  CHECK(h.is_error());
  CHECK(h.error() == "no");
}

TEST_CASE("value_or_abort overloads (success paths only) are instantiated")
{
  // Note: error paths abort the process; these tests only instantiate and call the success paths.

  g_abort_hook_calls = 0;

  Expected<int, std::string> ok(42);
  const Expected<int, std::string> cok(43);

  // const&
  CHECK(cok.value_or_abort(abort_hook) == 43);
  CHECK(g_abort_hook_calls == 0);

  // &
  CHECK(ok.value_or_abort(abort_hook) == 42);
  CHECK(g_abort_hook_calls == 0);

  // &&
  CHECK(Expected<int, std::string>(44).value_or_abort(abort_hook) == 44);
  CHECK(g_abort_hook_calls == 0);

  // const&&
  const Expected<int, std::string> cok2(45);
  CHECK(std::move(cok2).value_or_abort(abort_hook) == 45);
  CHECK(g_abort_hook_calls == 0);
}

TEST_CASE("move-only expected value still works with move APIs")
{
  Expected<MoveOnly, std::string> v(InPlace, 9);
  CHECK(v.is_expect());
  CHECK(v->v == 9);

  // move operator* and value()
  MoveOnly m1 = *std::move(v);
  CHECK(m1.v == 9);

  Expected<MoveOnly, std::string> v2(InPlace, 10);
  MoveOnly m2 = std::move(v2).value();
  CHECK(m2.v == 10);

  // map producing int
  Expected<MoveOnly, std::string> v3(InPlace, 11);
  auto mapped = std::move(v3).map([](MoveOnly&& m) { return m.v + 1; });
  CHECK(mapped.is_expect());
  CHECK(*mapped == 12);
}

TEST_CASE("operator! and explicit bool are instantiated")
{
  Expected<int, std::string> ok(1);
  Expected<int, std::string> err(Unexpected, std::string("e"));

  CHECK(!(!ok));
  CHECK(!!ok);

  CHECK((!err));
  CHECK(!(static_cast<bool>(err)));
}
