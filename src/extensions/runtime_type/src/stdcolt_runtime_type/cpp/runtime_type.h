/*****************************************************************/ /**
 * @file   runtime_type.h
 * @brief  Contains `Any`, `SharedAny` and `WeakAny`, C++ wrappers over `stdcolt_ext_rt_*Any`.
 * These classes provide an easier, modern API, and support type lookups
 * using templates.
 * @author Raphael Dib Nehme
 * @date   January 2026
 *********************************************************************/
#ifndef __HG_STDCOLT_EXT_RUNTIME_TYPE_CPP_RUNTIME_TYPE
#define __HG_STDCOLT_EXT_RUNTIME_TYPE_CPP_RUNTIME_TYPE

#include <optional>
#include <type_traits>
#include <utility>
#include <span>
#include <stdcolt_contracts/contracts.h>
#include <stdcolt_runtime_type/runtime_type.h>
#include <stdcolt_runtime_type/cpp/bindings.h>

/// @brief C++ wrappers and utilities over the `stdcolt_ext_rt` C API
namespace stdcolt::ext::rt
{
  /// @brief Reflect member information
  struct ReflectedMember
  {
    /// @brief The name of the member
    std::span<const char8_t> name;
    /// @brief The description of the member
    std::span<const char8_t> description;
    /// @brief The type of the member
    Type type;
    /// @brief The address or offset of the member
    uintptr_t address_or_offset;
  };

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

  /// @brief Any that may contain any type.
  /// This is a wrapper over the stable C API.
  class Any
  {
    /// @brief Underlying value
    stdcolt_ext_rt_Any _value;

    /// @brief Lookup a member using a function from the C API
    /// @tparam T The type of the member to lookup
    /// @tparam Self Any or const Any
    /// @tparam LookupFn `stdcolt_ext_rt_lookup_*`
    /// @param self This
    /// @param member The member name to lookup
    /// @return Pointer with correct const-ness, null on lookup failure
    template<typename T, auto LookupFn, class Self>
    static std::conditional_t<std::is_const_v<Self>, const T*, T*> lookup_impl(
        Self& self, std::u8string_view member) noexcept;

  public:
    /// @brief Constructs an empty value
    Any() noexcept { stdcolt_ext_rt_any_construct_empty(&_value); }
    /// @brief Destructor
    ~Any() noexcept { stdcolt_ext_rt_any_destroy(&_value); }
    /// @brief Move constructor
    /// @param other The value to move from
    Any(Any&& other) noexcept
    {
      stdcolt_ext_rt_any_construct_from_move(&_value, &other._value);
    }
    /// @brief Move assignment operator
    /// @param other The value to move from
    /// @return *this
    Any& operator=(Any&& other) noexcept
    {
      if (&other == this)
        return *this;

      stdcolt_ext_rt_any_destroy(&_value);
      stdcolt_ext_rt_any_construct_from_move(&_value, &other._value);
      return *this;
    }
    Any(const Any&)            = delete;
    Any& operator=(const Any&) = delete;

    /// @brief Constructs a value in place
    /// @tparam T The type of the value
    /// @tparam ...Ts The parameters for the constructors
    /// @param ctx The context in which to lookup for the type (not null!)
    /// @param ...args The arguments to forward to the constructor
    /// @return A value or nullopt on errors
    template<typename T, typename... Ts>
    static std::optional<Any> make_in_place(
        RuntimeContext* ctx, Ts&&... args) noexcept;

    /// @brief Check if the value is empty
    /// @return True if the value is empty
    bool is_empty() const noexcept { return _value.header.type == nullptr; }
    /// @brief Returns the type of the Any, or nullptr for empty values
    /// @return The type or nullptr for empty values
    Type type() const noexcept { return _value.header.type; }
    /// @brief Returns the context of the Any, or nullptr for empty values
    /// @return The context or nullptr for empty values
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

    /// @brief Returns the underlying handle
    /// @return The underlying handle
    const stdcolt_ext_rt_Any* c_handle() const noexcept { return &_value; }
    /// @brief Returns the underlying handle
    /// @return The underlying handle
    stdcolt_ext_rt_Any* c_handle() noexcept { return &_value; }

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
    /// @brief Cast the value to a pointer of a specific type.
    /// @tparam T The type to cast the value to
    /// @return nullptr if the type does not match, or valid pointer to T
    template<typename T>
    const T* as_type() const noexcept
    {
      if constexpr (std::is_array_v<T>)
        return is_type<T>() ? (const std::decay_t<T>)_value.header.address : nullptr;
      else
        return is_type<T>() ? (const T*)_value.header.address : nullptr;
    }
    /// @brief Cast the value to a pointer of a specific type.
    /// @tparam T The type to cast the value to
    /// @return nullptr if the type does not match, or valid pointer to T
    template<typename T>
    auto as_type() noexcept
    {
      if constexpr (std::is_array_v<T>)
        return is_type<T>() ? (std::decay_t<T>)_value.header.address : nullptr;
      else
        return is_type<T>() ? (T*)_value.header.address : nullptr;
    }

    /// @brief Converts the value to a span.
    /// For correct types:
    /// if the value contains an array, a span of the array
    /// size is returned; if the value contains a single
    /// value, a span of size one is returned.
    /// @tparam T The type or array type expected
    /// @return Empty span on invalid type, else span of either 1 or array size
    template<typename T>
    std::span<const T> as_span() const noexcept
    {
      auto t = type();
      if (t == nullptr)
        return {};
      if (t->kind == STDCOLT_EXT_RT_TYPE_KIND_ARRAY)
      {
        if (t->info.kind_array.array_type == type_of<T>(t->owner))
          return {(T*)base_address(), t->info.kind_array.size};
        return {};
      }
      if (t == type_of<T>(t->owner))
        return {(const T*)base_address(), 1};
      return {};
    }

    /// @brief Converts the value to a span.
    /// For correct types:
    /// if the value contains an array, a span of the array
    /// size is returned; if the value contains a single
    /// value, a span of size one is returned.
    /// @tparam T The type or array type expected
    /// @return Empty span on invalid type, else span of either 1 or array size
    template<typename T>
    std::span<T> as_span() noexcept
    {
      auto ret = ((const Any&)(*this)).as_span<T>();
      return {(T*)ret.data(), ret.size()};
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
    void reset() noexcept { stdcolt_ext_rt_any_destroy(&_value); }

    /// @brief Tries to copy the underlying stored object.
    /// This operation may fail due to memory allocation or if the
    /// object type is not actually copyable.
    /// @return nullopt if copy was not successful, else a copy of the stored object
    std::optional<Any> copy() const noexcept
    {
      Any val;
      if (stdcolt_ext_rt_any_construct_from_copy(&val._value, &_value)
          != STDCOLT_EXT_RT_VALUE_SUCCESS)
        return std::nullopt;
      return std::move(val);
    }

    /***************************/
    // REFLECTION API
    /***************************/

    /// @brief Returns an iterator over the reflected members of the type of the Any.
    /// @return Begin iterator over the reflected members of the current type
    reflection_iter reflect_begin() const noexcept
    {
      return reflection_iter(type());
    }
    /// @brief Returns an iterator over the reflected members of the type of the Any.
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

  class SharedAny
  {
    stdcolt_ext_rt_SharedAny _value{};

    template<typename T, auto LookupFn, class Self>
    static std::conditional_t<std::is_const_v<Self>, const T*, T*> lookup_impl(
        Self& self, std::u8string_view member) noexcept;

    friend class WeakAny;

  public:
    SharedAny() noexcept { stdcolt_ext_rt_sany_construct_empty(&_value); }
    ~SharedAny() noexcept { stdcolt_ext_rt_sany_destroy(&_value); }
    SharedAny(const SharedAny& other) noexcept
    {
      stdcolt_ext_rt_sany_construct_from_copy(&_value, &other._value);
    }
    SharedAny& operator=(const SharedAny& other) noexcept
    {
      if (this == &other)
        return *this;

      stdcolt_ext_rt_sany_destroy(&_value);
      stdcolt_ext_rt_sany_construct_from_copy(&_value, &other._value);
      return *this;
    }
    SharedAny(SharedAny&& other) noexcept
        : _value(other._value)
    {
      stdcolt_ext_rt_sany_construct_empty(&other._value);
    }
    SharedAny& operator=(SharedAny&& other) noexcept
    {
      if (this == &other)
        return *this;

      stdcolt_ext_rt_sany_destroy(&_value);
      _value = other._value;
      stdcolt_ext_rt_sany_construct_empty(&other._value);
      return *this;
    }

    template<typename T, typename... Ts>
    static std::optional<SharedAny> make_in_place(
        RuntimeContext* ctx, Ts&&... args) noexcept;

    bool is_empty() const noexcept { return _value.header.type == nullptr; }
    Type type() const noexcept { return _value.header.type; }

    RuntimeContext* context() const noexcept
    {
      return is_empty() ? nullptr : type()->owner;
    }

    void* base_address() noexcept { return _value.header.address; }
    const void* base_address() const noexcept { return _value.header.address; }

    const stdcolt_ext_rt_SharedAny* c_handle() const noexcept { return &_value; }
    stdcolt_ext_rt_SharedAny* c_handle() noexcept { return &_value; }

    void reset() noexcept { stdcolt_ext_rt_sany_destroy(&_value); }

    /***************************/
    // DIRECT-TYPE ACCESS
    /***************************/

    template<typename T>
    bool is_type() const noexcept
    {
      auto ctx = context();
      return ctx == nullptr ? false : type_of<T>(ctx) == type();
    }

    template<typename T>
    const T* as_type() const noexcept
    {
      if constexpr (std::is_array_v<T>)
        return is_type<T>() ? (const std::decay_t<T>)_value.header.address : nullptr;
      else
        return is_type<T>() ? (const T*)_value.header.address : nullptr;
    }

    template<typename T>
    auto as_type() noexcept
    {
      if constexpr (std::is_array_v<T>)
        return is_type<T>() ? (std::decay_t<T>)_value.header.address : nullptr;
      else
        return is_type<T>() ? (T*)_value.header.address : nullptr;
    }

    template<typename T>
    std::span<const T> as_span() const noexcept
    {
      auto t = type();
      if (t == nullptr)
        return {};
      if (t->kind == STDCOLT_EXT_RT_TYPE_KIND_ARRAY)
      {
        if (t->info.kind_array.array_type == type_of<T>(t->owner))
          return {(const T*)base_address(), t->info.kind_array.size};
        return {};
      }
      if (t == type_of<T>(t->owner))
        return {(const T*)base_address(), 1};
      return {};
    }

    template<typename T>
    std::span<T> as_span() noexcept
    {
      auto ret = ((const SharedAny&)(*this)).as_span<T>();
      return {(T*)ret.data(), ret.size()};
    }

    /***************************/
    // MEMBER LOOKUPS
    /***************************/

    template<typename T>
    const T* lookup_fast(std::u8string_view m) const noexcept
    {
      return lookup_impl<T, &stdcolt_ext_rt_type_lookup_fast>(*this, m);
    }
    template<typename T>
    T* lookup_fast(std::u8string_view m) noexcept
    {
      return lookup_impl<T, &stdcolt_ext_rt_type_lookup_fast>(*this, m);
    }
    template<typename T>
    const T* lookup(std::u8string_view m) const noexcept
    {
      return lookup_impl<T, &stdcolt_ext_rt_type_lookup>(*this, m);
    }
    template<typename T>
    T* lookup(std::u8string_view m) noexcept
    {
      return lookup_impl<T, &stdcolt_ext_rt_type_lookup>(*this, m);
    }

    /***************************/
    // REFLECTION API
    /***************************/

    /// @brief Returns an iterator over the reflected members of the type of the Any.
    /// @return Begin iterator over the reflected members of the current type
    reflection_iter reflect_begin() const noexcept
    {
      return reflection_iter(type());
    }
    /// @brief Returns an iterator over the reflected members of the type of the Any.
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

  class WeakAny
  {
    stdcolt_ext_rt_WeakAny _value{};

  public:
    WeakAny() noexcept
    {
      _value.address       = nullptr;
      _value.control_block = nullptr;
    }
    ~WeakAny() noexcept { stdcolt_ext_rt_wany_destroy(&_value); }
    WeakAny(const WeakAny& other) noexcept
    {
      stdcolt_ext_rt_wany_construct_from_copy(&_value, &other._value);
    }
    WeakAny& operator=(const WeakAny& other) noexcept
    {
      if (this == &other)
        return *this;

      stdcolt_ext_rt_wany_destroy(&_value);
      stdcolt_ext_rt_wany_construct_from_copy(&_value, &other._value);
      return *this;
    }
    WeakAny(WeakAny&& other) noexcept
        : _value(other._value)
    {
      other._value.address       = nullptr;
      other._value.control_block = nullptr;
    }
    WeakAny& operator=(WeakAny&& other) noexcept
    {
      if (this == &other)
        return *this;

      stdcolt_ext_rt_wany_destroy(&_value);
      _value                     = other._value;
      other._value.address       = nullptr;
      other._value.control_block = nullptr;
      return *this;
    }
    explicit WeakAny(const SharedAny& s) noexcept
    {
      stdcolt_ext_rt_wany_from_sany(&_value, &s._value);
    }
    WeakAny& operator=(const SharedAny& s) noexcept
    {
      stdcolt_ext_rt_wany_destroy(&_value);
      stdcolt_ext_rt_wany_from_sany(&_value, &s._value);
      return *this;
    }

    bool is_empty() const noexcept { return _value.control_block == nullptr; }

    void reset() noexcept { stdcolt_ext_rt_wany_destroy(&_value); }

    SharedAny try_lock() const noexcept
    {
      SharedAny out;
      stdcolt_ext_rt_wany_try_lock(out.c_handle(), &_value);
      return out;
    }

    SharedAny try_lock_consume() noexcept
    {
      SharedAny out;
      (void)stdcolt_ext_rt_wany_try_lock_consume(out.c_handle(), &_value);
      return out;
    }

    const stdcolt_ext_rt_WeakAny* c_handle() const noexcept { return &_value; }
    stdcolt_ext_rt_WeakAny* c_handle() noexcept { return &_value; }
  };

  template<typename T, typename... Ts>
  std::optional<Any> Any::make_in_place(RuntimeContext* ctx, Ts&&... args) noexcept
  {
    STDCOLT_pre(ctx != nullptr, "Context must not be null!");

    auto type = type_of<T>(ctx);
    if (!type)
      return std::nullopt;

    Any val;
    if (stdcolt_ext_rt_any_init(&val._value, type) != STDCOLT_EXT_RT_VALUE_SUCCESS)
      return std::nullopt;

    void* const storage = val._value.header.address;

    if constexpr (!std::is_array_v<T>)
    {
      if constexpr (std::is_nothrow_constructible_v<T, Ts...>)
      {
        std::construct_at(static_cast<T*>(storage), std::forward<Ts>(args)...);
        return val;
      }
      else
      {
        try
        {
          std::construct_at(static_cast<T*>(storage), std::forward<Ts>(args)...);
          return val;
        }
        catch (...)
        {
          stdcolt_ext_rt_any_destroy(&val._value);
          return std::nullopt;
        }
      }
    }
    else
    {
      static_assert(
          std::extent_v<T> != 0, "Unsupported: array of unknown bound T = U[]");

      using U            = std::remove_extent_t<T>;
      constexpr size_t N = std::extent_v<T>;

      static_assert(
          sizeof...(Ts) == N,
          "For U[N], make_in_place expects exactly N arguments (one per element).");

      U* const arr = static_cast<U*>(storage);

      if constexpr ((std::is_nothrow_constructible_v<U, Ts&&> && ...))
      {
        size_t i = 0;
        ((std::construct_at(arr + i++, std::forward<Ts>(args))), ...);
        return val;
      }
      else
      {
        size_t i = 0;
        try
        {
          ((std::construct_at(arr + i++, std::forward<Ts>(args))), ...);
          return val;
        }
        catch (...)
        {
          for (size_t j = 0; j < i; ++j)
            std::destroy_at(arr + j);

          stdcolt_ext_rt_any_destroy(&val._value);
          return std::nullopt;
        }
      }
    }
  }

  template<typename T, typename... Ts>
  std::optional<SharedAny> SharedAny::make_in_place(
      RuntimeContext* ctx, Ts&&... args) noexcept
  {
    STDCOLT_pre(ctx != nullptr, "Context must not be null!");

    Type t = type_of<T>(ctx);
    if (!t)
      return std::nullopt;

    SharedAny val;
    if (stdcolt_ext_rt_sany_init(&val._value, t) != STDCOLT_EXT_RT_VALUE_SUCCESS)
      return std::nullopt;

    void* const storage = val._value.header.address;

    if constexpr (!std::is_array_v<T>)
    {
      if constexpr (std::is_nothrow_constructible_v<T, Ts...>)
      {
        std::construct_at(static_cast<T*>(storage), std::forward<Ts>(args)...);
        return val;
      }
      else
      {
        try
        {
          std::construct_at(static_cast<T*>(storage), std::forward<Ts>(args)...);
          return val;
        }
        catch (...)
        {
          stdcolt_ext_rt_sany_destroy(&val._value);
          return std::nullopt;
        }
      }
    }
    else
    {
      static_assert(
          std::extent_v<T> != 0, "Unsupported: array of unknown bound T = U[]");

      using U            = std::remove_extent_t<T>;
      constexpr size_t N = std::extent_v<T>;

      static_assert(
          sizeof...(Ts) == N,
          "For U[N], make_in_place expects exactly N arguments.");

      U* const arr = static_cast<U*>(storage);

      if constexpr ((std::is_nothrow_constructible_v<U, Ts&&> && ...))
      {
        size_t i = 0;
        ((std::construct_at(arr + i++, std::forward<Ts>(args))), ...);
        return val;
      }
      else
      {
        size_t i = 0;
        try
        {
          ((std::construct_at(arr + i++, std::forward<Ts>(args))), ...);
          return val;
        }
        catch (...)
        {
          for (size_t j = 0; j < i; ++j)
            std::destroy_at(arr + j);

          stdcolt_ext_rt_sany_destroy(&val._value);
          return std::nullopt;
        }
      }
    }
  }

  template<typename T, auto LookupFn, class Self>
  std::conditional_t<std::is_const_v<Self>, const T*, T*> Any::lookup_impl(
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

  template<typename T, auto LookupFn, class Self>
  std::conditional_t<std::is_const_v<Self>, const T*, T*> SharedAny::lookup_impl(
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
