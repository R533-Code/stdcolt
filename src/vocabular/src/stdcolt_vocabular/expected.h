/*****************************************************************/ /**
 * @file   expected.h
 * @brief  Contains the `Expected` vocabulary type.
 * @author Raphael Dib Nehme
 * @date   January 2026
 *********************************************************************/
#ifndef __HG_STDCOLT_VOCABULARY_EXPECTED
#define __HG_STDCOLT_VOCABULARY_EXPECTED

#include <type_traits>
#include <utility>
#include <stdcolt_contracts/contracts.h>

namespace stdcolt
{
  /// @brief Tag struct for constructing errors in Expected
  struct unexpected_t
  {
  };

  /// @brief Tag object for constructing errors in Expected
  inline constexpr unexpected_t unexpected;

  /// @brief Tag struct for constructing an object in place
  struct in_place_t
  {
  };

  /// @brief Tag object for constructing an object in place
  inline constexpr in_place_t in_place;

  /// @brief A helper class that can hold either a value or an error.
  /// Example Usage:
  /// @code{.cpp}
  /// Expected<int, const char*> div(int a, int b)
  /// {
  ///   if (b != 0)
  ///     return a / b;
  ///   return { unexpected, "Division by zero is prohibited!" };
  /// }
  /// @endcode
  /// @tparam ExpectedTy The expected type
  /// @tparam ErrorTy The error type
  template<typename ExpectedTy, typename ErrorTy>
  class Expected
  {
    /// @brief Buffer for both error type and expected value
    union
    {
      /// @brief The expected value (active when is_error_v == false)
      ExpectedTy expected;
      /// @brief The error value (active when is_error_v == true)
      ErrorTy error_v;
    };

    /// @brief True if an error is stored in the Expected
    bool is_error_v;

  public:
    /// @brief Default constructs an error in the Expected
    /// @param  ErrorT tag
    constexpr Expected(unexpected_t) noexcept(
        std::is_nothrow_constructible_v<ErrorTy>)
        : is_error_v(true)
    {
      new (&error_v) ErrorTy();
    }

    /// @brief Copy constructs an error in the Expected
    /// @param  ErrorT tag
    /// @param value The value to copy
    constexpr Expected(unexpected_t, const ErrorTy& value) noexcept(
        std::is_nothrow_copy_constructible_v<ErrorTy>)
        : is_error_v(true)
    {
      new (&error_v) ErrorTy(value);
    }

    /// @brief Move constructs an error in the Expected
    /// @param  ErrorT tag
    /// @param to_move The value to move
    constexpr Expected(unexpected_t, ErrorTy&& to_move) noexcept(
        std::is_nothrow_move_constructible_v<ErrorTy>)
        : is_error_v(true)
    {
      new (&error_v) ErrorTy(std::move(to_move));
    }

    template<typename... Args>
    /// @brief Constructs an error in place in the Expected
    /// @tparam ...Args Parameter pack
    /// @param ...args Argument pack forwarded to the constructor
    constexpr Expected(in_place_t, unexpected_t, Args&&... args) noexcept(
        std::is_nothrow_constructible_v<ErrorTy, Args...>)
        : is_error_v(true)
    {
      new (&error_v) ErrorTy(std::forward<Args>(args)...);
    }

    /// @brief Default constructs an expected value in the Expected
    constexpr Expected() noexcept(std::is_default_constructible_v<ExpectedTy>)
        : is_error_v(false)
    {
      new (&expected) ExpectedTy();
    }

    /// @brief Copy constructs an expected value in the Expected
    /// @param value The value to copy
    constexpr Expected(const ExpectedTy& value) noexcept(
        std::is_nothrow_copy_constructible_v<ExpectedTy>)
        : is_error_v(false)
    {
      new (&expected) ExpectedTy(value);
    }

    /// @brief Move constructs an expected value in the Expected
    /// @param to_move The value to move
    constexpr Expected(ExpectedTy&& to_move) noexcept(
        std::is_nothrow_move_constructible_v<ExpectedTy>)
        : is_error_v(false)
    {
      new (&expected) ExpectedTy(std::move(to_move));
    }

    template<typename Ty, typename... Args>
      requires(!std::same_as<Ty, unexpected_t>)
    constexpr Expected(in_place_t, Ty&& arg, Args&&... args) noexcept(
        std::is_nothrow_constructible_v<ExpectedTy, Ty, Args...>)
        : is_error_v(false)
    {
      new (&expected) ExpectedTy(std::forward<Ty>(arg), std::forward<Args>(args)...);
    }

    /// @brief Copy constructs an Expected
    /// @param copy The Expected to copy
    constexpr Expected(const Expected& copy) noexcept(
        std::is_nothrow_copy_constructible_v<ExpectedTy>
        && std::is_nothrow_copy_constructible_v<ErrorTy>)
        : is_error_v(copy.is_error_v)
    {
      if (is_error_v)
        new (&error_v) ErrorTy(copy.error_v);
      else
        new (&expected) ExpectedTy(copy.expected);
    }

    /// @brief Copy assignment operator
    /// @param copy The Expected to copy
    /// @return Self
    constexpr Expected& operator=(const Expected& copy) noexcept(
        std::is_nothrow_copy_constructible_v<ExpectedTy>
        && std::is_nothrow_copy_constructible_v<ErrorTy>
        && std::is_nothrow_destructible_v<ErrorTy>
        && std::is_nothrow_destructible_v<ExpectedTy>)
    {
      if (&copy == this)
        return *this;

      if (is_error_v)
        error_v.~ErrorTy();
      else
        expected.~ExpectedTy();

      is_error_v = copy.is_error_v;
      if (is_error_v)
        new (&error_v) ErrorTy(copy.error_v);
      else
        new (&expected) ExpectedTy(copy.expected);

      return *this;
    }

    /// @brief Move constructs an Expected
    /// @param move The Expected to move
    constexpr Expected(Expected&& move) noexcept(
        std::is_nothrow_move_constructible_v<ExpectedTy>
        && std::is_nothrow_move_constructible_v<ErrorTy>)
        : is_error_v(move.is_error_v)
    {
      if (is_error_v)
        new (&error_v) ErrorTy(std::move(move.error_v));
      else
        new (&expected) ExpectedTy(std::move(move.expected));
    }

    /// @brief Move assignment operator
    /// @param move The Expected to move
    /// @return Self
    constexpr Expected& operator=(Expected&& move) noexcept(
        std::is_nothrow_move_constructible_v<ExpectedTy>
        && std::is_nothrow_move_constructible_v<ErrorTy>
        && std::is_nothrow_destructible_v<ErrorTy>
        && std::is_nothrow_destructible_v<ExpectedTy>)
    {
      if (&move == this)
        return *this;

      if (is_error_v)
        error_v.~ErrorTy();
      else
        expected.~ExpectedTy();

      is_error_v = move.is_error_v;
      if (is_error_v)
        new (&error_v) ErrorTy(std::move(move.error_v));
      else
        new (&expected) ExpectedTy(std::move(move.expected));

      return *this;
    }

    /// @brief Destructs the value/error contained in the Expected
    constexpr ~Expected() noexcept(
        std::is_nothrow_destructible_v<ExpectedTy>
        && std::is_nothrow_destructible_v<ErrorTy>)
    {
      if (is_error_v)
        error_v.~ErrorTy();
      else
        expected.~ExpectedTy();
    }

    /// @brief Check if the Expected contains an error
    /// @return True if the Expected contains an error
    constexpr bool is_error() const noexcept { return is_error_v; }
    /// @brief Check if the Expected contains an expected value
    /// @return True if the Expected contains an expected value
    constexpr bool is_expect() const noexcept { return !is_error_v; }

    /// @brief Check if the Expected contains an error.
    /// Same as is_error().
    /// @return True if the Expected contains an error
    constexpr bool operator!() const noexcept { return is_error_v; }
    /// @brief Check if the Expected contains an expected value.
    /// Same as is_expected().
    /// @return True if the Expected contains an expected value
    explicit constexpr operator bool() const noexcept { return !is_error_v; }

    /// @brief Returns the stored Expected value.
    /// @return The Expected value
    constexpr const ExpectedTy* operator->() const noexcept
    {
      STDCOLT_pre(is_expect(), "Expected contained an error!");
      return &expected;
    }

    /// @brief Returns the stored Expected value.
    /// @return The Expected value
    constexpr ExpectedTy* operator->() noexcept
    {
      STDCOLT_pre(is_expect(), "Expected contained an error!");
      return &expected;
    }

    /// @brief Returns the stored Expected value.
    /// @return The Expected value.
    constexpr const ExpectedTy& operator*() const& noexcept
    {
      STDCOLT_pre(is_expect(), "Expected contained an error!");
      return expected;
    }

    /// @brief Returns the stored Expected value.
    /// @return The Expected value.
    /// @pre is_expected() (colt_expected_is_expected).
    constexpr ExpectedTy& operator*() & noexcept
    {
      STDCOLT_pre(is_expect(), "Expected contained an error!");
      return expected;
    }

    /// @brief Returns the stored Expected value.
    /// @return The Expected value.
    constexpr const ExpectedTy&& operator*() const&& noexcept
    {
      STDCOLT_pre(is_expect(), "Expected contained an error!");
      return std::move(expected);
    }

    /// @brief Returns the stored Expected value.
    /// @return The Expected value.
    constexpr ExpectedTy&& operator*() && noexcept
    {
      STDCOLT_pre(is_expect(), "Expected contained an error!");
      return std::move(expected);
    }

    /// @brief Returns the stored Expected value.
    /// @return The Expected value.
    constexpr const ExpectedTy& value() const& noexcept
    {
      STDCOLT_pre(is_expect(), "Expected contained an error!");
      return expected;
    }

    /// @brief Returns the stored Expected value.
    /// @return The Expected value.
    constexpr ExpectedTy& value() & noexcept
    {
      STDCOLT_pre(is_expect(), "Expected contained an error!");
      return expected;
    }

    /// @brief Returns the stored Expected value.
    /// @return The Expected value.
    constexpr const ExpectedTy&& value() const&& noexcept
    {
      STDCOLT_pre(is_expect(), "Expected contained an error!");
      return std::move(expected);
    }

    /// @brief Returns the stored Expected value.
    /// @return The Expected value.
    constexpr ExpectedTy&& value() && noexcept
    {
      STDCOLT_pre(is_expect(), "Expected contained an error!");
      return std::move(expected);
    }

    /// @brief Returns the stored error value.
    /// @return The error value.
    constexpr const ErrorTy& error() const& noexcept
    {
      STDCOLT_pre(is_error(), "Expected did not contain an error!");
      return error_v;
    }

    /// @brief Returns the stored error value.
    /// @return The error value.
    constexpr ErrorTy& error() & noexcept
    {
      STDCOLT_pre(is_error(), "Expected did not contain an error!");
      return error_v;
    }

    /// @brief Returns the stored error value.
    /// @return The error value.
    constexpr const ErrorTy&& error() const&& noexcept
    {
      STDCOLT_pre(is_error(), "Expected did not contain an error!");
      return std::move(error_v);
    }

    /// @brief Returns the stored error value.
    /// @return The error value.
    constexpr ErrorTy&& error() && noexcept
    {
      STDCOLT_pre(is_error(), "Expected did not contain an error!");
      return std::move(error_v);
    }

    /// @brief Returns the Expected value if contained, else 'default_value'
    /// @param default_value The value to return if the Expected contains an error
    /// @return The Expected value or 'default_value'
    template<std::convertible_to<ExpectedTy> U>
    constexpr ExpectedTy value_or(U&& default_value) const&
    {
      return is_error_v ? static_cast<ExpectedTy>(std::forward<U>(default_value))
                        : **this;
    }
    /// @brief Returns the Expected value if contained, else 'default_value'
    /// @param default_value The value to return if the Expected contains an error
    /// @return The Expected value or 'default_value'
    template<std::convertible_to<ExpectedTy> U>
    constexpr ExpectedTy value_or(U&& default_value) &&
    {
      return is_error_v ? static_cast<ExpectedTy>(std::forward<U>(default_value))
                        : std::move(**this);
    }

    /********************************/
    // VALUE OR
    /********************************/

    template<typename Fn>
      requires std::invocable<Fn>
               && std::convertible_to<std::invoke_result_t<Fn>, ExpectedTy>
    constexpr ExpectedTy value_or(Fn&& default_value) const&
    {
      return is_error_v ? static_cast<ExpectedTy>(std::forward<Fn>(default_value)())
                        : **this;
    }

    template<typename Fn>
      requires std::invocable<Fn>
               && std::convertible_to<std::invoke_result_t<Fn>, ExpectedTy>
    constexpr ExpectedTy value_or(Fn&& default_value) &&
    {
      return is_error_v ? static_cast<ExpectedTy>(std::forward<Fn>(default_value)())
                        : std::move(**this);
    }

    /********************************/
    // OR ELSE
    /********************************/

    template<typename Fn>
      requires std::copy_constructible<ExpectedTy>
    constexpr auto or_else(Fn&& f) &
    {
      static_assert(
          std::invocable<Fn, decltype(this->error())>,
          "Function must have the error type as argument!");
      using G = std::remove_cvref_t<std::invoke_result_t<Fn, decltype(error())>>;
      if (is_expect())
        return G(in_place, **this);
      return std::invoke(std::forward<Fn>(f), error());
    }

    template<typename Fn>
      requires std::copy_constructible<ExpectedTy>
    constexpr auto or_else(Fn&& f) const&
    {
      static_assert(
          std::invocable<Fn, decltype(this->error())>,
          "Function must have the error type as argument!");
      using G = std::remove_cvref_t<std::invoke_result_t<Fn, decltype(error())>>;
      if (is_expect())
        return G(in_place, **this);
      return std::invoke(std::forward<Fn>(f), error());
    }
    template<typename Fn>
      requires std::move_constructible<ExpectedTy>
    constexpr auto or_else(Fn&& f) &&
    {
      static_assert(
          std::invocable<Fn, decltype(std::move(this->error()))>,
          "Function must have the error type as argument!");
      using G = std::remove_cvref_t<
          std::invoke_result_t<Fn, decltype(std::move(error()))>>;
      if (is_expect())
        return G(in_place, std::move(**this));
      return std::invoke(std::forward<Fn>(f), std::move(error()));
    }

    template<typename Fn>
      requires std::move_constructible<ExpectedTy>
    constexpr auto or_else(Fn&& f) const&&
    {
      static_assert(
          std::invocable<Fn, decltype(std::move(this->error()))>,
          "Function must have the error type as argument!");
      using G = std::remove_cvref_t<
          std::invoke_result_t<Fn, decltype(std::move(error()))>>;
      if (is_expect())
        return G(in_place, std::move(**this));
      return std::invoke(std::forward<Fn>(f), std::move(error()));
    }

    /********************************/
    // AND_THEN
    /********************************/

    template<typename F>
    constexpr auto and_then(F&& f) &
    {
      static_assert(
          std::invocable<F, decltype(**this)>,
          "Function must have the value type as argument!");
      using U = std::remove_cvref_t<std::invoke_result_t<F, decltype(**this)>>;
      if (is_expect())
        return std::invoke(std::forward<F>(f), **this);
      return U(unexpected, error());
    }

    template<typename F>
    constexpr auto and_then(F&& f) const&
    {
      static_assert(
          std::invocable<F, decltype(**this)>,
          "Function must have the value type as argument!");
      using U = std::remove_cvref_t<std::invoke_result_t<F, decltype(**this)>>;
      if (is_expect())
        return std::invoke(std::forward<F>(f), **this);
      return U(unexpected, error());
    }

    template<typename F>
    constexpr auto and_then(F&& f) &&
    {
      static_assert(
          std::invocable<F, decltype(std::move(**this))>,
          "Function must have the value type as argument!");
      using U =
          std::remove_cvref_t<std::invoke_result_t<F, decltype(std::move(**this))>>;
      if (is_expect())
        return std::invoke(std::forward<F>(f), std::move(**this));
      return U(unexpected, std::move(error()));
    }

    template<typename F>
    constexpr auto and_then(F&& f) const&&
    {
      static_assert(
          std::invocable<F, decltype(std::move(**this))>,
          "Function must have the value type as argument!");
      using U =
          std::remove_cvref_t<std::invoke_result_t<F, decltype(std::move(**this))>>;
      if (is_expect())
        return std::invoke(std::forward<F>(f), std::move(**this));
      return U(unexpected, std::move(error()));
    }

    /********************************/
    // MAP
    /********************************/

    template<typename F>
    constexpr auto map(F&& f) &
    {
      static_assert(
          std::invocable<F, decltype(**this)>,
          "Function must have the value type as argument!");
      using U = std::remove_cvref_t<std::invoke_result_t<F, decltype(**this)>>;
      if (is_expect())
        return Expected<U, ErrorTy>(std::invoke(std::forward<F>(f), **this));
      return Expected<U, ErrorTy>(unexpected, error());
    }

    template<typename F>
    constexpr auto map(F&& f) const&
    {
      static_assert(
          std::invocable<F, decltype(**this)>,
          "Function must have the value type as argument!");
      using U = std::remove_cvref_t<std::invoke_result_t<F, decltype(**this)>>;
      if (is_expect())
        return Expected<U, ErrorTy>(std::invoke(std::forward<F>(f), **this));
      return Expected<U, ErrorTy>(unexpected, error());
    }

    template<typename F>
    constexpr auto map(F&& f) &&
    {
      static_assert(
          std::invocable<F, decltype(std::move(**this))>,
          "Function must have the value type as argument!");
      using U =
          std::remove_cvref_t<std::invoke_result_t<F, decltype(std::move(**this))>>;
      if (is_expect())
        return Expected<U, ErrorTy>(
            std::invoke(std::forward<F>(f), std::move(**this)));
      return Expected<U, ErrorTy>(unexpected, std::move(error()));
    }

    template<typename F>
    constexpr auto map(F&& f) const&&
    {
      static_assert(
          std::invocable<F, decltype(std::move(**this))>,
          "Function must have the value type as argument!");
      using U =
          std::remove_cvref_t<std::invoke_result_t<F, decltype(std::move(**this))>>;
      if (is_expect())
        return Expected<U, ErrorTy>(
            std::invoke(std::forward<F>(f), std::move(**this)));
      return Expected<U, ErrorTy>(unexpected, std::move(error()));
    }

    /********************************/
    // VALUE_OR_ABORT
    /********************************/

    /// @brief Returns the expected value, or aborts if it does not exist.
    /// @param on_abort The function to call before aborting or null
    /// @return The expected value
    constexpr const ExpectedTy& value_or_abort(
        void (*on_abort)(void) noexcept = nullptr) const& noexcept
    {
      if (is_error_v)
      {
        if (on_abort)
          on_abort();
        std::abort();
      }
      else
        return expected;
    }

    /// @brief Returns the expected value, or aborts if it does not exist.
    /// @param on_abort The function to call before aborting or null
    /// @return The expected value
    constexpr ExpectedTy& value_or_abort(
        void (*on_abort)(void) noexcept = nullptr) & noexcept
    {
      if (is_error_v)
      {
        if (on_abort)
          on_abort();
        std::abort();
      }
      else
        return expected;
    }

    /// @brief Returns the expected value, or aborts if it does not exist.
    /// @param on_abort The function to call before aborting or null
    /// @return The expected value
    constexpr const ExpectedTy&& value_or_abort(
        void (*on_abort)(void) noexcept = nullptr) const&& noexcept
    {
      if (is_error_v)
      {
        if (on_abort)
          on_abort();
        std::abort();
      }
      else
        return expected;
    }
    /// @brief Returns the expected value, or aborts if it does not exist.
    /// @param on_abort The function to call before aborting or null
    /// @return The expected value
    constexpr ExpectedTy&& value_or_abort(
        void (*on_abort)(void) noexcept = nullptr) && noexcept
    {
      if (is_error_v)
      {
        if (on_abort)
          on_abort();
        std::abort();
      }
      else
        return std::move(expected);
    }
  };
} // namespace stdcolt

#endif // !__HG_STDCOLT_VOCABULARY_EXPECTED
