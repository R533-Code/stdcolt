#ifndef __HG_STDCOLT_EXT_RUNTIME_TYPE_CPP_BINDINGS
#define __HG_STDCOLT_EXT_RUNTIME_TYPE_CPP_BINDINGS

#include <stdcolt_runtime_type/runtime_type.h>
#include <stdcolt_runtime_type/cpp/abi_thunk.h>
#include <string_view>
#include <memory>

namespace stdcolt::ext::rt
{
  /// @brief Opaque type ID
  using OpaqueTypeID = stdcolt_ext_rt_OpaqueTypeID;

  namespace detail
  {
    template<bool OWNING>
    class RuntimeContextBase
    {
      static constexpr bool IS_OWNING = OWNING;
      static constexpr bool IS_VIEW   = !OWNING;

      stdcolt_ext_rt_RuntimeContext* _ptr;

      RuntimeContextBase(stdcolt_ext_rt_RuntimeContext* val) noexcept
          : _ptr(val)
      {
      }

      template<bool>
      friend class RuntimeContextBase;

    public:
      RuntimeContextBase() = delete;
      // owning vvv
      RuntimeContextBase(RuntimeContextBase&& other) noexcept
        requires IS_OWNING
          : _ptr(std::exchange(other._ptr, nullptr))
      {
      }
      RuntimeContextBase& operator=(RuntimeContextBase&& other) noexcept
        requires IS_OWNING
      {
        std::swap(other._ptr, _ptr);
        return *this;
      }
      RuntimeContextBase(const RuntimeContextBase&) noexcept
        requires IS_OWNING
      = delete;
      RuntimeContextBase& operator=(const RuntimeContextBase&) noexcept
        requires IS_OWNING
      = delete;
      ~RuntimeContextBase() noexcept
        requires IS_OWNING
      {
        stdcolt_ext_rt_destroy(_ptr);
      }
      // non-owning vvv
      RuntimeContextBase(RuntimeContextBase&&) noexcept
        requires IS_VIEW
      = default;
      RuntimeContextBase& operator=(RuntimeContextBase&&) noexcept
        requires IS_VIEW
      = default;
      RuntimeContextBase(const RuntimeContextBase&) noexcept
        requires IS_VIEW
      = default;
      RuntimeContextBase& operator=(const RuntimeContextBase&) noexcept
        requires IS_VIEW
      = default;
      ~RuntimeContextBase() noexcept
        requires IS_VIEW
      = default;

      static std::optional<RuntimeContextBase> make() noexcept
        requires IS_OWNING
      {
        auto ret = stdcolt_ext_rt_create(nullptr, nullptr);
        if (ret.result == STDCOLT_EXT_RT_CTX_SUCCESS)
          return RuntimeContextBase(ret.data.success.context);
        return std::nullopt;
      }
      /// @brief Transfers the ownership of a C handle to the RAII object.
      /// @param to_adopt The handle whose ownership to adopt
      /// @return nullopt on null
      static std::optional<RuntimeContextBase> adopt(
          stdcolt_ext_rt_RuntimeContext* to_adopt) noexcept
        requires IS_OWNING
      {
        if (to_adopt == nullptr)
          return std::nullopt;
        return RuntimeContextBase(to_adopt);
      }
      static RuntimeContextBase unchecked_from_handle(
          stdcolt_ext_rt_RuntimeContext* handle) noexcept
        requires IS_VIEW
      {
        STDCOLT_debug_pre(handle != nullptr, "expected non-null handle!");
        return RuntimeContextBase(handle);
      }
      static std::optional<RuntimeContextBase> from_handle(
          stdcolt_ext_rt_RuntimeContext* handle) noexcept
        requires IS_VIEW
      {
        if (handle == nullptr)
          return std::nullopt;
        return RuntimeContextBase(handle);
      }

      /// @brief Returns the underlying handle
      /// @return The underlying handle
      auto c_handle() noexcept
      {
        STDCOLT_pre(_ptr != nullptr, "using moved-from object!");
        return _ptr;
      }

      RuntimeContextBase<false> as_view() noexcept
        requires IS_OWNING
      {
        STDCOLT_pre(_ptr != nullptr, "using moved-from object!");
        return RuntimeContextBase<false>(_ptr);
      }
      operator RuntimeContextBase<false>() noexcept
        requires IS_OWNING
      {
        return as_view();
      }
      operator stdcolt_ext_rt_RuntimeContext*() noexcept { return c_handle(); }
    };
  } // namespace detail

  /// @brief RAII wrapper over RuntimeContext
  using RuntimeContext = detail::RuntimeContextBase<true>;
  /// @brief View over a RuntimeContext.
  /// @note Always pass by value.
  using RuntimeContextView = detail::RuntimeContextBase<false>;

  class TypeReflectionIterator;
  struct TypeReflector;

  /// @brief Member kind
  enum class MemberKind : uint8_t
  {
    /// @brief Non-static data field
    MEMBER_FIELD = STDCOLT_EXT_RT_MEMBER_FIELD,
    /// @brief Static data field
    MEMBER_STATIC_FIELD = STDCOLT_EXT_RT_MEMBER_STATIC_FIELD,
    /// @brief Non-static method
    MEMBER_METHOD = STDCOLT_EXT_RT_MEMBER_METHOD,
  };

  class TypeView
  {
    stdcolt_ext_rt_Type _type;

  public:
    TypeView() noexcept
        : _type(nullptr)
    {
    }
    TypeView(stdcolt_ext_rt_Type t) noexcept
        : _type(t)
    {
    }

    explicit operator bool() const noexcept { return _type != nullptr; }
    bool operator==(const TypeView&) const noexcept = default;
    bool operator==(std::nullptr_t) const noexcept { return _type == nullptr; }

    stdcolt_ext_rt_Type c_handle() const noexcept { return _type; }
    operator stdcolt_ext_rt_Type() const noexcept { return _type; }

    /***************************/
    // COMMON ACCESSORS
    /***************************/

    stdcolt_ext_rt_TypeKind kind() const noexcept
    {
      STDCOLT_pre(_type != nullptr, "null TypeView");
      return _type->kind;
    }

    RuntimeContextView owner() const noexcept
    {
      STDCOLT_pre(_type != nullptr, "null TypeView");
      return RuntimeContextView::unchecked_from_handle(_type->owner);
    }

    uint64_t size() const noexcept
    {
      STDCOLT_pre(_type != nullptr, "null TypeView");
      return _type->type_size;
    }

    uint64_t alignment() const noexcept
    {
      STDCOLT_pre(_type != nullptr, "null TypeView");
      return _type->type_align;
    }

    /***************************/
    // KIND PREDICATES
    /***************************/

    bool is_named() const noexcept
    {
      return _type && _type->kind == STDCOLT_EXT_RT_TYPE_KIND_NAMED;
    }
    bool is_builtin() const noexcept
    {
      return _type && _type->kind == STDCOLT_EXT_RT_TYPE_KIND_BUILTIN;
    }
    bool is_array() const noexcept
    {
      return _type && _type->kind == STDCOLT_EXT_RT_TYPE_KIND_ARRAY;
    }
    bool is_pointer() const noexcept
    {
      return _type && _type->kind == STDCOLT_EXT_RT_TYPE_KIND_POINTER;
    }
    bool is_function() const noexcept
    {
      return _type && _type->kind == STDCOLT_EXT_RT_TYPE_KIND_FUNCTION;
    }

    /***************************/
    // LIFETIME PREDICATES
    /***************************/

    bool is_trivially_copyable() const noexcept
    {
      return _type && _type->trivial_copyable;
    }
    bool is_trivially_movable() const noexcept
    {
      return _type && _type->trivial_movable;
    }
    bool is_trivially_destructible() const noexcept
    {
      return _type && _type->trivial_destroy;
    }
    bool is_copyable() const noexcept
    {
      return _type && (_type->trivial_copyable || _type->has_copy_fn);
    }
    bool is_movable() const noexcept
    {
      return _type && (_type->trivial_movable || _type->has_move_fn);
    }

    /***************************/
    // NAMED TYPE
    /***************************/

    std::u8string_view name() const noexcept
    {
      STDCOLT_pre(is_named(), "type is not named");
      auto sv = stdcolt_ext_rt_reflect_name(_type);
      return {(const char8_t*)sv.data, sv.size};
    }

    TypeReflectionIterator reflect_begin() const noexcept;
    TypeReflectionIterator reflect_end() const noexcept;
    TypeReflector reflect() const noexcept;

    /***************************/
    // BUILTIN TYPE
    /***************************/

    stdcolt_ext_rt_BuiltInType builtin_kind() const noexcept
    {
      STDCOLT_pre(is_builtin(), "type is not a builtin");
      return _type->info.kind_builtin.type;
    }

    /***************************/
    // ARRAY TYPE
    /***************************/

    TypeView element_type() const noexcept
    {
      STDCOLT_pre(is_array(), "type is not an array");
      return _type->info.kind_array.array_type;
    }

    uint64_t array_length() const noexcept
    {
      STDCOLT_pre(is_array(), "type is not an array");
      return _type->info.kind_array.size;
    }

    /***************************/
    // POINTER TYPE
    /***************************/

    TypeView pointee_type() const noexcept
    {
      STDCOLT_pre(is_pointer(), "type is not a pointer");
      return _type->info.kind_pointer.pointee_type;
    }

    bool pointee_is_const() const noexcept
    {
      STDCOLT_pre(is_pointer(), "type is not a pointer");
      return (bool)_type->info.kind_pointer.pointee_is_const;
    }

    /***************************/
    // FUNCTION TYPE
    /***************************/

    // null TypeView means void return
    TypeView return_type() const noexcept
    {
      STDCOLT_pre(is_function(), "type is not a function");
      return _type->info.kind_function.return_type;
    }

    uint64_t argument_count() const noexcept
    {
      STDCOLT_pre(is_function(), "type is not a function");
      return _type->info.kind_function.argument_count;
    }

    TypeView argument(uint64_t i) const noexcept
    {
      STDCOLT_pre(is_function(), "type is not a function");
      STDCOLT_pre(
          i < _type->info.kind_function.argument_count, "index out of range");
      return static_cast<const stdcolt_ext_rt_Type*>(
          _type->info.kind_function.argument_types)[i];
    }

    std::span<const TypeView> arguments() const noexcept
    {
      STDCOLT_pre(is_function(), "type is not a function");
      return {
          reinterpret_cast<const TypeView*>(
              _type->info.kind_function.argument_types),
          static_cast<size_t>(_type->info.kind_function.argument_count)};
    }
  };

  /// @brief Reflect member information
  struct ReflectedMember
  {
    /// @brief The name of the member
    std::u8string_view name;
    /// @brief The description of the member
    std::u8string_view description;
    /// @brief The type of the member
    TypeView type;
    /// @brief The member kind
    MemberKind kind;
    /// @brief The address or offset of the member
    uintptr_t address_or_offset;
  };

  /// @brief Iterator over the reflected members of a type
  class TypeReflectionIterator
  {
    stdcolt_ext_rt_ReflectIterator* _iter = nullptr;

  public:
    /// @brief Constructs an end `reflection_iter`
    TypeReflectionIterator() noexcept = default;
    /// @brief Constructs a begin `reflection_iter` from a typ
    /// @param type The type or null
    TypeReflectionIterator(TypeView type) noexcept
        : _iter(stdcolt_ext_rt_reflect_create(type))
    {
      // advances the iterator so that the first read is valid
      _iter = stdcolt_ext_rt_reflect_advance(_iter);
    }
    /// @brief Destructor
    ~TypeReflectionIterator() noexcept
    {
      if (_iter)
        stdcolt_ext_rt_reflect_destroy(_iter);
    }
    TypeReflectionIterator(TypeReflectionIterator&& other) noexcept
        : _iter(std::exchange(other._iter, nullptr))
    {
    }
    TypeReflectionIterator& operator=(TypeReflectionIterator&& other) noexcept
    {
      if (this == &other)
        return *this;
      stdcolt_ext_rt_reflect_destroy(_iter);
      _iter = std::exchange(other._iter, nullptr);
      return *this;
    }
    TypeReflectionIterator(const TypeReflectionIterator&)            = delete;
    TypeReflectionIterator& operator=(const TypeReflectionIterator&) = delete;

    /// @brief Advances the iterator
    /// @return Self
    TypeReflectionIterator& operator++() noexcept
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
          (MemberKind)member.kind,
          member.address_or_offset};
    }

    bool operator==(const TypeReflectionIterator&) const noexcept = default;
    bool operator!=(const TypeReflectionIterator&) const noexcept = default;
  };

  struct TypeReflector
  {
    TypeView t;
    TypeReflectionIterator begin() const noexcept
    {
      return TypeReflectionIterator(t);
    }
    TypeReflectionIterator end() const noexcept { return {}; }
  };

  inline TypeReflectionIterator TypeView::reflect_begin() const noexcept
  {
    STDCOLT_pre(is_named(), "type is not named");
    return TypeReflectionIterator(_type);
  }

  inline TypeReflectionIterator TypeView::reflect_end() const noexcept
  {
    return {};
  }

  inline TypeReflector TypeView::reflect() const noexcept
  {
    STDCOLT_pre(is_named(), "type is not named");
    return TypeReflector{*this};
  }

  /// @brief Member description
  using Member = stdcolt_ext_rt_Member;
  /// @brief Member info description
  using MemberInfo = stdcolt_ext_rt_MemberInfo;
  /// @brief Type erased functions to manage the lifetime of an object
  using NamedLifetime = stdcolt_ext_rt_NamedLifetime;
  /// @brief
  using ResultType = stdcolt_ext_rt_ResultType;
  /// @brief
  using ResultLookup = stdcolt_ext_rt_ResultLookup;

  /// @brief Contains a global variable that is unique per type per module.
  /// In different modules (such as DLLs), this generates different addresses
  /// for the same types.
  /// @tparam T The type
  template<typename T>
  struct opaque_type_tag
  {
    /// @brief The address of this variable is unique per type per module
    static inline const char value = 0;
  };

  /// @brief Returns a unique OpaqueTypeID per type per module.
  /// @tparam T The type
  /// @return OpaqueTypeID unique per type per module
  template<typename T>
  OpaqueTypeID unique_opaque_type_id_for() noexcept
  {
    return &opaque_type_tag<T>::value;
  }

  /***********************/
  // TYPE TAGS
  /***********************/

  template<typename T>
  struct type_tag
  {
    static TypeView get(RuntimeContextView ctx) noexcept
    {
      return stdcolt_ext_rt_register_get_type(
          ctx, unique_opaque_type_id_for<std::remove_cv_t<T>>());
    }
  };

  template<typename T>
  TypeView type_of(RuntimeContextView ctx) noexcept
  {
    return type_tag<T>::get(ctx);
  }

  template<>
  struct type_tag<void>
  {
    static TypeView get(RuntimeContextView) noexcept { return nullptr; }
  };

  template<>
  struct type_tag<void*>
  {
    static TypeView get(RuntimeContextView ctx) noexcept
    {
      if (auto ret = stdcolt_ext_rt_type_create_builtin(
              ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_OPAQUE_ADDRESS);
          ret.result == STDCOLT_EXT_RT_TYPE_SUCCESS)
        return ret.data.success.type;
      return nullptr;
    }
  };

  template<>
  struct type_tag<const void*>
  {
    static TypeView get(RuntimeContextView ctx) noexcept
    {
      if (auto ret = stdcolt_ext_rt_type_create_builtin(
              ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_CONST_OPAQUE_ADDRESS);
          ret.result == STDCOLT_EXT_RT_TYPE_SUCCESS)
        return ret.data.success.type;
      return nullptr;
    }
  };

  template<>
  struct type_tag<bool>
  {
    static TypeView get(RuntimeContextView ctx) noexcept
    {
      if (auto ret = stdcolt_ext_rt_type_create_builtin(
              ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_BOOL);
          ret.result == STDCOLT_EXT_RT_TYPE_SUCCESS)
        return ret.data.success.type;
      return nullptr;
    }
  };

  template<>
  struct type_tag<uint8_t>
  {
    static TypeView get(RuntimeContextView ctx) noexcept
    {
      if (auto ret = stdcolt_ext_rt_type_create_builtin(
              ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_U8);
          ret.result == STDCOLT_EXT_RT_TYPE_SUCCESS)
        return ret.data.success.type;
      return nullptr;
    }
  };
  template<>
  struct type_tag<uint16_t>
  {
    static TypeView get(RuntimeContextView ctx) noexcept
    {
      if (auto ret = stdcolt_ext_rt_type_create_builtin(
              ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_U16);
          ret.result == STDCOLT_EXT_RT_TYPE_SUCCESS)
        return ret.data.success.type;
      return nullptr;
    }
  };
  template<>
  struct type_tag<uint32_t>
  {
    static TypeView get(RuntimeContextView ctx) noexcept
    {
      if (auto ret = stdcolt_ext_rt_type_create_builtin(
              ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_U32);
          ret.result == STDCOLT_EXT_RT_TYPE_SUCCESS)
        return ret.data.success.type;
      return nullptr;
    }
  };
  template<>
  struct type_tag<uint64_t>
  {
    static TypeView get(RuntimeContextView ctx) noexcept
    {
      if (auto ret = stdcolt_ext_rt_type_create_builtin(
              ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_U64);
          ret.result == STDCOLT_EXT_RT_TYPE_SUCCESS)
        return ret.data.success.type;
      return nullptr;
    }
  };
  template<>
  struct type_tag<int8_t>
  {
    static TypeView get(RuntimeContextView ctx) noexcept
    {
      if (auto ret = stdcolt_ext_rt_type_create_builtin(
              ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_I8);
          ret.result == STDCOLT_EXT_RT_TYPE_SUCCESS)
        return ret.data.success.type;
      return nullptr;
    }
  };
  template<>
  struct type_tag<int16_t>
  {
    static TypeView get(RuntimeContextView ctx) noexcept
    {
      if (auto ret = stdcolt_ext_rt_type_create_builtin(
              ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_I16);
          ret.result == STDCOLT_EXT_RT_TYPE_SUCCESS)
        return ret.data.success.type;
      return nullptr;
    }
  };
  template<>
  struct type_tag<int32_t>
  {
    static TypeView get(RuntimeContextView ctx) noexcept
    {
      if (auto ret = stdcolt_ext_rt_type_create_builtin(
              ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_I32);
          ret.result == STDCOLT_EXT_RT_TYPE_SUCCESS)
        return ret.data.success.type;
      return nullptr;
    }
  };
  template<>
  struct type_tag<int64_t>
  {
    static TypeView get(RuntimeContextView ctx) noexcept
    {
      if (auto ret = stdcolt_ext_rt_type_create_builtin(
              ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_I64);
          ret.result == STDCOLT_EXT_RT_TYPE_SUCCESS)
        return ret.data.success.type;
      return nullptr;
    }
  };
  template<>
  struct type_tag<char>
  {
    static TypeView get(RuntimeContextView ctx) noexcept
    {
      if constexpr (std::is_signed_v<char>)
        return type_of<int8_t>(ctx);
      else
        return type_of<uint8_t>(ctx);
    }
  };
  template<>
  struct type_tag<float>
  {
    static TypeView get(RuntimeContextView ctx) noexcept
    {
      if (auto ret = stdcolt_ext_rt_type_create_builtin(
              ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_FLOAT);
          ret.result == STDCOLT_EXT_RT_TYPE_SUCCESS)
        return ret.data.success.type;
      return nullptr;
    }
  };
  template<>
  struct type_tag<double>
  {
    static TypeView get(RuntimeContextView ctx) noexcept
    {
      if (auto ret = stdcolt_ext_rt_type_create_builtin(
              ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_DOUBLE);
          ret.result == STDCOLT_EXT_RT_TYPE_SUCCESS)
        return ret.data.success.type;
      return nullptr;
    }
  };
  template<typename T>
  struct type_tag<const T> : type_tag<T>
  {
  };
  template<typename T>
  struct type_tag<volatile T> : type_tag<T>
  {
  };
  template<typename T>
  struct type_tag<const volatile T> : type_tag<T>
  {
  };

  template<typename T>
  struct type_tag<T&>
  {
    static TypeView get(RuntimeContextView ctx) noexcept { return type_of<T*>(ctx); }
  };
  template<typename T>
  struct type_tag<const T&>
  {
    static TypeView get(RuntimeContextView ctx) noexcept
    {
      return type_of<const T*>(ctx);
    }
  };
  template<typename T>
  struct type_tag<T&&>
  {
    static TypeView get(RuntimeContextView ctx) noexcept { return type_of<T*>(ctx); }
  };

  template<typename T>
  struct type_tag<T*>
  {
    static TypeView get(RuntimeContextView ctx) noexcept
    {
      if constexpr (std::is_void_v<T>)
        return type_of<void*>(ctx);
      else
      {
        if (auto ret = stdcolt_ext_rt_type_create_ptr(ctx, type_of<T>(ctx), false);
            ret.result == STDCOLT_EXT_RT_TYPE_SUCCESS)
          return ret.data.success.type;
        return nullptr;
      }
    }
  };
  template<typename T>
  struct type_tag<const T*>
  {
    static TypeView get(RuntimeContextView ctx) noexcept
    {
      if constexpr (std::is_void_v<T>)
        return type_of<const void*>(ctx);
      else
      {
        if (auto ret = stdcolt_ext_rt_type_create_ptr(ctx, type_of<T>(ctx), true);
            ret.result == STDCOLT_EXT_RT_TYPE_SUCCESS)
          return ret.data.success.type;
        return nullptr;
      }
    }
  };

  template<class T, size_t N>
  struct type_tag<T[N]>
  {
    static TypeView get(RuntimeContextView ctx) noexcept
    {
      using Elem = std::remove_cv_t<T>;

      TypeView elem = type_of<Elem>(ctx);
      if (!elem)
        return nullptr;

      auto r = stdcolt_ext_rt_type_create_array(ctx, elem, (uint64_t)N);
      if (r.result == STDCOLT_EXT_RT_TYPE_SUCCESS)
        return r.data.success.type;
      return nullptr;
    }
  };
  template<class T, size_t N>
  struct type_tag<const T[N]> : type_tag<T[N]>
  {
  };
  template<class T, size_t N>
  struct type_tag<volatile T[N]> : type_tag<T[N]>
  {
  };
  template<class T, size_t N>
  struct type_tag<const volatile T[N]> : type_tag<T[N]>
  {
  };

  template<class R, class... A>
  struct type_tag<R (*)(A...)>
  {
    static TypeView get(RuntimeContextView ctx) noexcept;
  };

  template<class R, class... A>
  TypeView type_of_fn(RuntimeContextView ctx) noexcept
  {
    if constexpr (sizeof...(A) == 0)
    {
      TypeView ret = type_of<R>(ctx);
      if (auto _ret = stdcolt_ext_rt_type_create_fn(ctx, ret, {nullptr, 0});
          _ret.result == STDCOLT_EXT_RT_TYPE_SUCCESS)
        return _ret.data.success.type;
      return nullptr;
    }
    else
    {
      TypeView ret               = type_of<R>(ctx);
      stdcolt_ext_rt_Type argv[] = {type_of<A>(ctx)...};
      if (auto _ret = stdcolt_ext_rt_type_create_fn(ctx, ret, {argv, sizeof...(A)});
          _ret.result == STDCOLT_EXT_RT_TYPE_SUCCESS)
        return _ret.data.success.type;
      return nullptr;
    }
  }

  template<class R, class... A>
  TypeView type_tag<R (*)(A...)>::get(RuntimeContextView ctx) noexcept
  {
    return type_of_fn<R, A...>(ctx);
  }

  /***********************/
  // C++ BINDINGS
  /***********************/

  template<auto PMF>
  struct method_thunk;

  // non-const method
  template<typename C, class R, class... A, R (C::*PMF)(A...)>
  struct method_thunk<PMF>
  {
    using Fn = abi_ret_t<R> (*)(void*, abi_param_t<A>...);

    static abi_ret_t<R> call(void* self, abi_param_t<A>... a) noexcept(
        noexcept((((C*)self)->*PMF)(from_abi<A>(a)...)))
    {
      auto&& r = (((C*)self)->*PMF)(from_abi<A>(a)...);
      return to_abi_ret<R>(static_cast<R&&>(r));
    }

    static TypeView signature(RuntimeContextView ctx) noexcept
    {
      return type_of_fn<abi_ret_t<R>, void*, abi_param_t<A>...>(ctx);
    }
  };

  template<typename C, class R, class... A, R (C::*PMF)(A...) const>
  struct method_thunk<PMF>
  {
    using Fn = abi_ret_t<R> (*)(const void*, abi_param_t<A>...);

    static abi_ret_t<R> call(const void* self, abi_param_t<A>... a) noexcept(
        noexcept((((const C*)self)->*PMF)(from_abi<A>(a)...)))
    {
      auto&& r = (((const C*)self)->*PMF)(from_abi<A>(a)...);
      return to_abi_ret<R>(static_cast<R&&>(r));
    }

    static TypeView signature(RuntimeContextView ctx) noexcept
    {
      return type_of_fn<abi_ret_t<R>, const void*, abi_param_t<A>...>(ctx);
    }
  };

  template<auto PMF>
  struct method
  {
    std::u8string_view nm;
    std::u8string_view doc{};

    constexpr method(std::u8string_view n, std::u8string_view d = {})
        : nm(n)
        , doc(d)
    {
    }

    Member make_member(RuntimeContextView ctx) const noexcept
    {
      Member m{};
      m.name              = {(const char*)nm.data(), nm.size()};
      m.description       = {(const char*)doc.data(), doc.size()};
      m.address_or_offset = (uintptr_t)(&method_thunk<PMF>::call);
      m.kind              = STDCOLT_EXT_RT_MEMBER_METHOD;
      m.type              = method_thunk<PMF>::signature(ctx);
      return m;
    }
  };

  template<typename Owner, class FieldT, size_t Offset>
  struct field
  {
    std::u8string_view nm;
    std::u8string_view doc{};

    constexpr field(std::u8string_view n, std::u8string_view d = {})
        : nm(n)
        , doc(d)
    {
    }

    Member make_member(RuntimeContextView ctx) const noexcept
    {
      static_assert(
          std::is_standard_layout_v<Owner>,
          "offsetof is only supported for standard-layout types");

      Member m{};
      m.name              = {(const char*)nm.data(), nm.size()};
      m.description       = {(const char*)doc.data(), doc.size()};
      m.kind              = STDCOLT_EXT_RT_MEMBER_FIELD;
      m.address_or_offset = (uintptr_t)Offset;
      m.type              = type_of<FieldT>(ctx);
      return m;
    }
  };

  template<typename D>
  concept MemberDescriptor = requires(const D& d, RuntimeContextView ctx) {
    { d.make_member(ctx) } noexcept -> std::same_as<Member>;
  };

  template<typename T>
  static void destroy_T(TypeView, void* p) noexcept
  {
    std::destroy_at(reinterpret_cast<T*>(p));
  }

  template<typename T>
  static bool copy_T(TypeView, void* out, const void* src) noexcept
  {
    try
    {
      std::construct_at(reinterpret_cast<T*>(out), *reinterpret_cast<const T*>(src));
      return true;
    }
    catch (...)
    {
      return false;
    }
  }

  template<typename T>
  static void move_T(TypeView, void* out, void* src) noexcept
  {
    auto* s = reinterpret_cast<T*>(src);
    std::construct_at(reinterpret_cast<T*>(out), std::move(*s));
    std::destroy_at(s);
  }

  template<typename T>
  static constexpr NamedLifetime make_lifetime() noexcept
  {
    NamedLifetime lt{};
    lt.move_fn               = nullptr;
    lt.copy_fn               = nullptr;
    lt.destroy_fn            = nullptr;
    lt.is_trivially_movable  = 0;
    lt.is_trivially_copyable = 0;

    // Destroy
    if constexpr (!std::is_trivially_destructible_v<T>)
      lt.destroy_fn = &destroy_T<T>;

    // Copy
    if constexpr (std::is_trivially_copyable_v<T>)
    {
      lt.copy_fn               = nullptr;
      lt.is_trivially_copyable = 1;
    }
    else if constexpr (std::is_copy_constructible_v<T>)
    {
      lt.copy_fn               = &copy_T<T>;
      lt.is_trivially_copyable = 0;
    }
    else
    {
      lt.copy_fn               = nullptr;
      lt.is_trivially_copyable = 0;
    }

    // Move
    if constexpr (
        std::is_trivially_move_constructible_v<T>
        && std::is_trivially_destructible_v<T>)
    {
      lt.move_fn              = nullptr;
      lt.is_trivially_movable = 1;
    }
    else if constexpr (std::is_nothrow_move_constructible_v<T>)
    {
      lt.move_fn              = &move_T<T>;
      lt.is_trivially_movable = 0;
    }
    else
    {
      // Cannot provide move_fn because it must be noexcept.
      lt.move_fn              = nullptr;
      lt.is_trivially_movable = 0;
    }

    return lt;
  }

  template<typename T, MemberDescriptor... Ts>
  ResultType bind_type(
      RuntimeContextView ctx, std::u8string_view name, Ts&&... ts) noexcept
  {
    Member members[sizeof...(Ts)]{};
    size_t i = 0;
    ((members[i++] = ts.make_member(ctx)), ...);
    static constexpr NamedLifetime lt = make_lifetime<std::remove_cv_t<T>>();

    auto _name    = stdcolt_ext_rt_StringView{(const char*)name.data(), name.size()};
    auto _members = stdcolt_ext_rt_MemberView{members, sizeof...(Ts)};
    auto t        = stdcolt_ext_rt_type_create(
        ctx, &_name, &_members, alignof(T), sizeof(T), &lt, nullptr, nullptr);
    if (t.result == STDCOLT_EXT_RT_TYPE_SUCCESS)
    {
      (void)stdcolt_ext_rt_register_set_type(
          ctx, unique_opaque_type_id_for<std::remove_cv_t<T>>(),
          t.data.success.type);
    }
    return t;
  }
} // namespace stdcolt::ext::rt

#define STDCOLT_RT_FIELD(Owner, member, name_u8, doc_u8)             \
  ::stdcolt::ext::rt::field<                                         \
      Owner, decltype(((Owner*)0)->member), offsetof(Owner, member)> \
  {                                                                  \
    name_u8, doc_u8                                                  \
  }

#define STDCOLT_RT_METHOD(Owner, method_name, name_u8, doc_u8) \
  ::stdcolt::ext::rt::method<&Owner::method_name>              \
  {                                                            \
    (name_u8), (doc_u8)                                        \
  }

#endif // !__HG_STDCOLT_EXT_RUNTIME_TYPE_CPP_BINDINGS
