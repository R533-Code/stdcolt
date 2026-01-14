#ifndef __HG_STDCOLT_EXT_RUNTIME_TYPE_CPP_BINDINGS
#define __HG_STDCOLT_EXT_RUNTIME_TYPE_CPP_BINDINGS

#include <stdcolt_runtime_type/runtime_type.h>
#include <stdcolt_runtime_type/cpp/abi_thunk.h>
#include <string_view>
#include <memory>

namespace stdcolt::ext::rt
{
  /// @brief Runtime type description
  using Type = stdcolt_ext_rt_Type;
  /// @brief Opaque type ID
  using OpaqueTypeID = stdcolt_ext_rt_OpaqueTypeID;
  /// @brief RuntimeContext
  using RuntimeContext = stdcolt_ext_rt_RuntimeContext;
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
    static Type get(RuntimeContext* ctx) noexcept
    {
      return stdcolt_ext_rt_register_get_type(
          ctx, unique_opaque_type_id_for<std::remove_cv_t<T>>());
    }
  };

  template<typename T>
  Type type_of(RuntimeContext* ctx) noexcept
  {
    return type_tag<T>::get(ctx);
  }

  template<>
  struct type_tag<void>
  {
    static Type get(RuntimeContext*) noexcept { return nullptr; }
  };

  template<>
  struct type_tag<void*>
  {
    static Type get(RuntimeContext* ctx) noexcept
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
    static Type get(RuntimeContext* ctx) noexcept
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
    static Type get(RuntimeContext* ctx) noexcept
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
    static Type get(RuntimeContext* ctx) noexcept
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
    static Type get(RuntimeContext* ctx) noexcept
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
    static Type get(RuntimeContext* ctx) noexcept
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
    static Type get(RuntimeContext* ctx) noexcept
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
    static Type get(RuntimeContext* ctx) noexcept
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
    static Type get(RuntimeContext* ctx) noexcept
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
    static Type get(RuntimeContext* ctx) noexcept
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
    static Type get(RuntimeContext* ctx) noexcept
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
    static Type get(RuntimeContext* ctx) noexcept
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
    static Type get(RuntimeContext* ctx) noexcept
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
    static Type get(RuntimeContext* ctx) noexcept
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
    static Type get(RuntimeContext* ctx) noexcept { return type_of<T*>(ctx); }
  };
  template<typename T>
  struct type_tag<const T&>
  {
    static Type get(RuntimeContext* ctx) noexcept { return type_of<const T*>(ctx); }
  };
  template<typename T>
  struct type_tag<T&&>
  {
    static Type get(RuntimeContext* ctx) noexcept { return type_of<T*>(ctx); }
  };

  template<typename T>
  struct type_tag<T*>
  {
    static Type get(RuntimeContext* ctx) noexcept
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
    static Type get(RuntimeContext* ctx) noexcept
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
    static Type get(RuntimeContext* ctx) noexcept
    {
      using Elem = std::remove_cv_t<T>;

      Type elem = type_of<Elem>(ctx);
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
    static Type get(RuntimeContext* ctx) noexcept;
  };

  template<class R, class... A>
  Type type_of_fn(RuntimeContext* ctx) noexcept
  {
    Type ret    = type_of<R>(ctx);
    Type argv[] = {type_of<A>(ctx)...};
    if (auto _ret = stdcolt_ext_rt_type_create_fn(ctx, ret, {argv, sizeof...(A)});
        _ret.result == STDCOLT_EXT_RT_TYPE_SUCCESS)
      return _ret.data.success.type;
    return nullptr;
  }

  template<class R, class... A>
  Type type_tag<R (*)(A...)>::get(RuntimeContext* ctx) noexcept
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

    static Type signature(RuntimeContext* ctx) noexcept
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

    static Type signature(RuntimeContext* ctx) noexcept
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

    Member make_member(RuntimeContext* ctx) const noexcept
    {
      Member m{};
      m.name              = {nm.data(), nm.size()};
      m.description       = {doc.data(), doc.size()};
      m.address_or_offset = (uintptr_t)(&method_thunk<PMF>::call);
      m.type              = method_thunk<PMF>::signature(ctx);
      return m;
    }
  };

  template<typename Owner, class FieldT, std::size_t Offset>
  struct field
  {
    std::u8string_view nm;
    std::u8string_view doc{};

    constexpr field(std::u8string_view n, std::u8string_view d = {})
        : nm(n)
        , doc(d)
    {
    }

    Member make_member(RuntimeContext* ctx) const noexcept
    {
      static_assert(
          std::is_standard_layout_v<Owner>,
          "offsetof is only supported for standard-layout types");

      Member m{};
      m.name              = {(const char*)nm.data(), nm.size()};
      m.description       = {(const char*)doc.data(), doc.size()};
      m.address_or_offset = (uintptr_t)Offset;
      m.type              = type_of<FieldT>(ctx);
      return m;
    }
  };

  template<typename D>
  concept MemberDescriptor = requires(const D& d, RuntimeContext* ctx) {
    { d.make_member(ctx) } noexcept -> std::same_as<Member>;
  };

  template<typename T>
  static void destroy_T(Type, void* p) noexcept
  {
    std::destroy_at(reinterpret_cast<T*>(p));
  }

  template<typename T>
  static bool copy_T(Type, void* out, const void* src) noexcept
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
  static void move_T(Type, void* out, void* src) noexcept
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
      RuntimeContext* ctx, std::u8string_view name, Ts&&... ts) noexcept
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
