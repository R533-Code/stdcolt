#ifndef __HG_STDCOLT_EXT_RUNTIME_TYPE_CPP_RUNTIME_TYPE
#define __HG_STDCOLT_EXT_RUNTIME_TYPE_CPP_RUNTIME_TYPE

#include <optional>
#include <type_traits>
#include <utility>
#include <span>
#include <stdcolt_contracts/contracts.h>
#include <stdcolt_runtime_type/runtime_type.h>
#include <stdcolt_runtime_type/cpp/bindings.h>

namespace stdcolt::ext::rt
{
  struct ReflectedMember
  {
    std::span<const char8_t> name;
    std::span<const char8_t> description;
    Type type;
    uintptr_t address_or_offset;
  };

  /// @brief Value that may contain any type.
  /// This is a wrapper over the stable C API.
  class Value
  {
    /// @brief Underlying value
    stdcolt_ext_rt_Value _value;

    template<typename T, auto LookupFn, class Self>
    static std::conditional_t<std::is_const_v<Self>, const T*, T*> lookup_impl(
        Self& self, std::u8string_view member) noexcept;

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
        Ts&&... args) noexcept(std::is_nothrow_constructible_v<T, Ts...>);

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

    /***************************/
    // DIRECT-TYPE ACCESS
    /***************************/

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

    /***************************/
    // MEMBER LOOKUPS
    /***************************/

    /// @brief Lookup a member
    /// @tparam T The type of the member
    /// @param m The name of the member
    /// @return Pointer to the member
    template<typename T>
    const T* lookup_fast(std::u8string_view m) const noexcept
    {
      return lookup_impl<T, &stdcolt_ext_rt_type_lookup_fast>(*this, m);
    }
    /// @brief Lookup a member
    /// @tparam T The type of the member
    /// @param m The name of the member
    /// @return Pointer to the member
    template<typename T>
    T* lookup_fast(std::u8string_view m) noexcept
    {
      return lookup_impl<T, &stdcolt_ext_rt_type_lookup_fast>(*this, m);
    }
    /// @brief Lookup a member
    /// @tparam T The type of the member
    /// @param m The name of the member
    /// @return Pointer to the member
    template<typename T>
    const T* lookup(std::u8string_view m) const noexcept
    {
      return lookup_impl<T, &stdcolt_ext_rt_type_lookup>(*this, m);
    }
    /// @brief Lookup a member
    /// @tparam T The type of the member
    /// @param m The name of the member
    /// @return Pointer to the member
    template<typename T>
    T* lookup(std::u8string_view m) noexcept
    {
      return lookup_impl<T, &stdcolt_ext_rt_type_lookup>(*this, m);
    }

    /***************************/
    // LIFETIME
    /***************************/

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

    /***************************/
    // REFLECTION API
    /***************************/

    /// @brief Iterator over the reflected members of a type
    class reflection_iter
    {
      stdcolt_ext_rt_ReflectIterator* _iter = nullptr;

    public:
      /// @brief Constructs an end `reflection_iter`
      reflection_iter() noexcept = default;
      /// @brief Constructs a begin `reflection_iter` from a typ
      /// @param type The type or null
      reflection_iter(Type type) noexcept
          : _iter(stdcolt_ext_rt_reflect_create(type))
      {
        // advances the iterator so that the first read is valid
        _iter = stdcolt_ext_rt_reflect_advance(_iter);
      }
      /// @brief Destructor
      ~reflection_iter() noexcept
      {
        if (_iter)
          stdcolt_ext_rt_reflect_destroy(_iter);
      }
      reflection_iter(reflection_iter&&) noexcept            = default;
      reflection_iter& operator=(reflection_iter&&) noexcept = default;
      reflection_iter(const reflection_iter&)                = delete;
      reflection_iter& operator=(const reflection_iter&)     = delete;

      /// @brief Advances the iterator
      /// @return Self
      reflection_iter& operator++() noexcept
      {
        _iter = stdcolt_ext_rt_reflect_advance(_iter);
        return *this;
      }

      /// @brief Returns the reflected member
      /// @return ReflectedMember
      ReflectedMember operator*() const noexcept
      {
        STDCOLT_pre(_iter != nullptr, "dereferencing invalid iterator!");
        auto member = stdcolt_ext_rt_reflect_read(_iter);
        return {
            {(const char8_t*)member.name.data, member.name.size},
            {(const char8_t*)member.description.data, member.description.size},
            member.type,
            member.address_or_offset};
      }

      bool operator==(const reflection_iter&) const noexcept = default;
      bool operator!=(const reflection_iter&) const noexcept = default;
    };

    /// @brief Returns an iterator over the reflected members of the type of the Value.
    /// @return Begin iterator over the reflected members of the current type
    reflection_iter reflect_begin() const noexcept
    {
      return reflection_iter(type());
    }
    /// @brief Returns an iterator over the reflected members of the type of the Value.
    /// @return End iterator over the reflected members of the current type
    reflection_iter reflect_end() const noexcept { return reflection_iter(); }
    /// @brief Returns an object with `begin` and `end` methods to reflect
    /// on the members of a type.
    /// @return Object with `begin` and `end` for reflection
    auto reflect() const noexcept
    {
      struct reflector
      {
        Type reflected_type;

        reflection_iter begin() const noexcept
        {
          return reflection_iter(reflected_type);
        }
        reflection_iter end() const noexcept { return reflection_iter(); }
      };
      return reflector{type()};
    }
    /// @brief Returns the name of type if it is named
    /// @return Name of the type for named type or an empty string view
    std::u8string_view reflect_name() const noexcept
    {
      auto _type = type();
      if (_type == nullptr || _type->kind != STDCOLT_EXT_RT_TYPE_KIND_NAMED)
        return {};
      auto ret = stdcolt_ext_rt_reflect_name(_type);
      return {(const char8_t*)ret.data, ret.size};
    }
  };

  template<typename T, typename... Ts>
  std::optional<Value> Value::make_in_place(
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

  template<typename T, auto LookupFn, class Self>
  std::conditional_t<std::is_const_v<Self>, const T*, T*> Value::lookup_impl(
      Self& self, std::u8string_view member) noexcept
  {
    if (self.is_empty())
      return nullptr;

    auto name = stdcolt_ext_rt_StringView{
        reinterpret_cast<const char*>(member.data()), member.size()};

    auto res = LookupFn(self.type(), &name, type_of<T>(self.context()));
    if (res.result != STDCOLT_EXT_RT_LOOKUP_FOUND)
      return nullptr;

    auto base = static_cast<const char*>(self.base_address());
    auto addr = base + res.data.found.address_or_offset;

    using Ptr = std::conditional_t<std::is_const_v<Self>, const T*, T*>;
    return reinterpret_cast<Ptr>(const_cast<char*>(addr));
  }
} // namespace stdcolt::ext::rt

#endif // !__HG_STDCOLT_EXT_RUNTIME_TYPE_CPP_RUNTIME_TYPE
