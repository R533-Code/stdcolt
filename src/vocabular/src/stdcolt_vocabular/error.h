/*****************************************************************/ /**
 * @file   error.h
 * @brief  Contains `Error`, that represents a success or an error.
 * @author Raphael Dib Nehme
 * @date   January 2026
 *********************************************************************/
#ifndef __HG_STDCOLT_VOCABULARY_ERROR
#define __HG_STDCOLT_VOCABULARY_ERROR

#include <stdcolt_contracts/contracts.h>
#include <cstdint>

namespace stdcolt
{
  /// @brief Represents either a success or an error that must be checked
  /// before its destructor runs.
  /// Its ABI layout is guaranteed to not change, even on configuration changes.
  class Error
  {
    /// @brief True if an error
    uint8_t _is_error : 1 = 0;
    /// @brief True if the error was consumed (only used in debug)
    mutable uint8_t _is_consumed : 1 = 0;
    /// @brief Unused bits
    uint8_t _ : 6 = 0;

    /// @brief Constructs an error
    /// @param is_error True if error, false if success
    constexpr Error(bool is_error) noexcept
        : _is_error(is_error)
    {
    }

    /// @brief Marks the error as consumed
    constexpr void consume() const noexcept { _is_consumed = true; }
    /// @brief Asserts that the error is consumed
    constexpr void check_consume() const noexcept
    {
      STDCOLT_assert(_is_consumed, "Error was not checked");
    }

  public:
    /// @brief Move constructor
    /// @param other The error to move from
    constexpr Error(Error&& other) noexcept
        : _is_error(other._is_error)
        , _is_consumed(other._is_consumed)
        , _(other._)
    {
      other.consume();
    }
    /// @brief Move assignment operator
    /// @param other The value to move from
    /// @return this
    constexpr Error& operator=(Error&& other) noexcept
    {
      if (this == &other)
        return *this;
      auto a            = *(uint8_t*)this;
      *(uint8_t*)this   = *(uint8_t*)&other;
      *(uint8_t*)&other = a;
      return *this;
    }
    /// @brief Destructor
    constexpr ~Error() noexcept { check_consume(); }

    /// @brief Creates an error
    /// @return Error for which `is_error` returns true.
    static constexpr Error error() noexcept { return Error{true}; }
    /// @brief Creates a success
    /// @return Error for which `is_error` is false
    static constexpr Error success() noexcept { return Error{false}; }

    /// @brief Returns true if `Error` is an error
    /// @return True if `Error` is an error
    constexpr bool is_error() const noexcept
    {
      consume();
      return _is_error;
    }
    /// @brief Returns true if `Error` is a success (not an error!)
    /// @return True if `Error` is a success
    constexpr bool is_success() const noexcept { return !is_error(); }
    /// @brief Check if success
    constexpr explicit operator bool() const noexcept { return is_success(); }

    /// @brief Drops the error
    constexpr void drop() const noexcept { consume(); }
  };
} // namespace stdcolt

#endif // !__HG_STDCOLT_VOCABULARY_ERROR
