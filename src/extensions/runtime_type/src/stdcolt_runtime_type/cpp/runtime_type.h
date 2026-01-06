#ifndef __HG_STDCOLT_EXT_RUNTIME_TYPE_CPP_RUNTIME_TYPE
#define __HG_STDCOLT_EXT_RUNTIME_TYPE_CPP_RUNTIME_TYPE

#include <optional>
#include <type_traits>
#include <utility>
#include <stdcolt_contracts/contracts.h>
#include <stdcolt_runtime_type/runtime_type.h>
#include <stdcolt_runtime_type/cpp/bindings.h>

namespace stdcolt::ext::rt
{
  /// @brief Value that may contain any type.
  /// This is a wrapper of the C stable API.
  class Value
  {
    /// @brief Underlying value
    stdcolt_ext_rt_Value _value;

  public:
    /// @brief Constructs an empty value
    Value() noexcept { stdcolt_ext_rt_val_construct_empty(&_value); }
    /// @brief Destructor
    ~Value() noexcept { stdcolt_ext_rt_val_destroy(&_value); }
    /// @brief Move constructor
    /// @param other The value to move from
    Value(Value&& other) noexcept
    {
      stdcolt_ext_rt_val_construct_from_move(&_value, &other._value);
    }
    /// @brief Move assignment operator
    /// @param other The value to move from
    /// @return *this
    Value& operator=(Value&& other) noexcept
    {
      if (&other == this)
        return *this;

      stdcolt_ext_rt_val_destroy(&_value);
      stdcolt_ext_rt_val_construct_from_move(&_value, &other._value);
      return *this;
    }
    Value(const Value&)            = delete;
    Value& operator=(const Value&) = delete;

    /// @brief Constructs a value in place
    /// @tparam T The type of the value
    /// @tparam ...Ts The parameters for the constructors
    /// @param ctx The context in which to lookup for the type (not null!)
    /// @param ...args The arguments to forward to the constructor
    /// @return A value or nullopt on errors
    template<typename T, typename... Ts>
    static std::optional<Value> make_in_place(
        RuntimeContext* ctx,
        Ts&&... args) noexcept(std::is_nothrow_constructible_v<T, Ts...>)
    {
      STDCOLT_pre(ctx != nullptr, "Context must not be null!");

      auto type = type_of<T>(ctx);
      if (type == nullptr)
        return std::nullopt;

      Value val;
      if (stdcolt_ext_rt_val_construct(&val._value, type)
          != STDCOLT_EXT_RT_VALUE_SUCCESS)
        return std::nullopt;

      if constexpr (std::is_nothrow_constructible_v<T, Ts...>)
        new (val._value.header.address) T(std::forward<Ts>(args)...);
      else
      {
        try
        {
          new (val._value.header.address) T(std::forward<Ts>(args)...);
        }
        catch (...)
        {
          return std::nullopt;
        }
      }
      return std::move(val);
    }

    /// @brief Check if the value is empty
    /// @return True if the value is empty
    bool is_empty() const noexcept { return _value.header.type == nullptr; }
    /// @brief Returns the type of the Value, or nullptr for empty values
    /// @return The type or nullptr for empty values
    Type type() const noexcept { return _value.header.type; }
    /// @brief Returns the context of the Value, or nullptr for empty values
    /// @return The context of nullptr for empty values
    RuntimeContext* context() const noexcept
    {
      return is_empty() ? nullptr : type()->owner;
    }
    /// @brief Base address of the object stored or nullptr if empty
    /// @return Base address or nullptr
    void* base_address() noexcept { return _value.header.address; }
    /// @brief Base address of the object stored or nullptr if empty
    /// @return Base address or nullptr
    const void* base_address() const noexcept { return _value.header.address; }

    /// @brief Check if the value contains a specific type
    /// @tparam T The type to check for
    /// @return True if the value contains an object of that type
    template<typename T>
    bool is_type() const noexcept
    {
      auto ctx = context();
      return ctx == nullptr ? false : type_of<T>(ctx) == type();
    }

    template<typename T>
    const T* as_type() const noexcept
    {
      return is_type<T>() ? (const T*)_value.header.address : nullptr;
    }
    template<typename T>
    T* as_type() noexcept
    {
      return is_type<T>() ? (T*)_value.header.address : nullptr;
    }

    template<typename T>
    const T* lookup_fast(std::u8string_view member) const noexcept
    {
      if (is_empty())
        return nullptr;
      auto res = stdcolt_ext_rt_type_lookup_fast(
          type(), {(const char*)member.data(), member.size()}, type_of<T>());
      if (res.result == STDCOLT_EXT_RT_LOOKUP_FOUND)
        return (const T*)((const char*)base_address()
                          + res.data.found.address_or_offset);
      return nullptr;
    }
    template<typename T>
    T* lookup_fast(std::u8string_view member) noexcept
    {
      if (is_empty())
        return nullptr;
      auto res = stdcolt_ext_rt_type_lookup_fast(
          type(), {(const char*)member.data(), member.size()}, type_of<T>());
      if (res.result == STDCOLT_EXT_RT_LOOKUP_FOUND)
        return (T*)((const char*)base_address() + res.data.found.address_or_offset);
      return nullptr;
    }

    /// @brief Destroys the stored object, and mark the value as empty
    void reset() noexcept { stdcolt_ext_rt_val_destroy(&_value); }

    /// @brief Tries to copy the underlying stored object.
    /// This operation may fail due to memory allocation or if the
    /// object type is not actually copyable.
    /// @return nullopt if copy was not successful, else a copy of the stored object
    std::optional<Value> copy() const noexcept
    {
      Value val;
      if (stdcolt_ext_rt_val_construct_from_copy(&val._value, &_value)
          != STDCOLT_EXT_RT_VALUE_SUCCESS)
        return std::nullopt;
      return std::move(val);
    }
  };
} // namespace stdcolt::ext::rt

#endif // !__HG_STDCOLT_EXT_RUNTIME_TYPE_CPP_RUNTIME_TYPE
