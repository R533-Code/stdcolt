#ifndef __HG_STDCOLT_EXT_RUNTIME_TYPE_CPP_BINDINGS
#define __HG_STDCOLT_EXT_RUNTIME_TYPE_CPP_BINDINGS

#include <stdcolt_runtime_type/runtime_type.h>
#include <stdcolt_runtime_type/cpp_abi_thunk.h>

namespace stdcolt::ext::rt
{
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

  template<class T>
  struct type_tag
  {
    static Type get(RuntimeContext* ctx) noexcept
    {
      return rt_register_get_type(
          ctx, unique_opaque_type_id_for<std::remove_cv_t<T>>());
    }
  };

  template<class T>
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
      if (auto ret = rt_type_create_builtin(ctx, BuiltInType::TYPE_OPAQUE_ADDRESS);
          ret.result == TypeResult::TYPE_SUCCESS)
        return ret.success.type;
      return nullptr;
    }
  };

  template<>
  struct type_tag<const void*>
  {
    static Type get(RuntimeContext* ctx) noexcept
    {
      if (auto ret =
              rt_type_create_builtin(ctx, BuiltInType::TYPE_CONST_OPAQUE_ADDRESS);
          ret.result == TypeResult::TYPE_SUCCESS)
        return ret.success.type;
      return nullptr;
    }
  };

  template<>
  struct type_tag<bool>
  {
    static Type get(RuntimeContext* ctx) noexcept
    {
      if (auto ret = rt_type_create_builtin(ctx, BuiltInType::TYPE_BOOL);
          ret.result == TypeResult::TYPE_SUCCESS)
        return ret.success.type;
      return nullptr;
    }
  };

  template<>
  struct type_tag<uint8_t>
  {
    static Type get(RuntimeContext* ctx) noexcept
    {
      if (auto ret = rt_type_create_builtin(ctx, BuiltInType::TYPE_U8);
          ret.result == TypeResult::TYPE_SUCCESS)
        return ret.success.type;
      return nullptr;
    }
  };
  template<>
  struct type_tag<uint16_t>
  {
    static Type get(RuntimeContext* ctx) noexcept
    {
      if (auto ret = rt_type_create_builtin(ctx, BuiltInType::TYPE_U16);
          ret.result == TypeResult::TYPE_SUCCESS)
        return ret.success.type;
      return nullptr;
    }
  };
  template<>
  struct type_tag<uint32_t>
  {
    static Type get(RuntimeContext* ctx) noexcept
    {
      if (auto ret = rt_type_create_builtin(ctx, BuiltInType::TYPE_U32);
          ret.result == TypeResult::TYPE_SUCCESS)
        return ret.success.type;
      return nullptr;
    }
  };
  template<>
  struct type_tag<uint64_t>
  {
    static Type get(RuntimeContext* ctx) noexcept
    {
      if (auto ret = rt_type_create_builtin(ctx, BuiltInType::TYPE_U64);
          ret.result == TypeResult::TYPE_SUCCESS)
        return ret.success.type;
      return nullptr;
    }
  };
  template<>
  struct type_tag<int8_t>
  {
    static Type get(RuntimeContext* ctx) noexcept
    {
      if (auto ret = rt_type_create_builtin(ctx, BuiltInType::TYPE_I8);
          ret.result == TypeResult::TYPE_SUCCESS)
        return ret.success.type;
      return nullptr;
    }
  };
  template<>
  struct type_tag<int16_t>
  {
    static Type get(RuntimeContext* ctx) noexcept
    {
      if (auto ret = rt_type_create_builtin(ctx, BuiltInType::TYPE_I16);
          ret.result == TypeResult::TYPE_SUCCESS)
        return ret.success.type;
      return nullptr;
    }
  };
  template<>
  struct type_tag<int32_t>
  {
    static Type get(RuntimeContext* ctx) noexcept
    {
      if (auto ret = rt_type_create_builtin(ctx, BuiltInType::TYPE_I32);
          ret.result == TypeResult::TYPE_SUCCESS)
        return ret.success.type;
      return nullptr;
    }
  };
  template<>
  struct type_tag<int64_t>
  {
    static Type get(RuntimeContext* ctx) noexcept
    {
      if (auto ret = rt_type_create_builtin(ctx, BuiltInType::TYPE_I64);
          ret.result == TypeResult::TYPE_SUCCESS)
        return ret.success.type;
      return nullptr;
    }
  };
  template<>
  struct type_tag<float>
  {
    static Type get(RuntimeContext* ctx) noexcept
    {
      if (auto ret = rt_type_create_builtin(ctx, BuiltInType::TYPE_FLOAT);
          ret.result == TypeResult::TYPE_SUCCESS)
        return ret.success.type;
      return nullptr;
    }
  };
  template<>
  struct type_tag<double>
  {
    static Type get(RuntimeContext* ctx) noexcept
    {
      if (auto ret = rt_type_create_builtin(ctx, BuiltInType::TYPE_DOUBLE);
          ret.result == TypeResult::TYPE_SUCCESS)
        return ret.success.type;
      return nullptr;
    }
  };

  template<class T>
  struct type_tag<const T> : type_tag<T>
  {
  };

  template<class T>
  struct type_tag<volatile T> : type_tag<T>
  {
  };
  template<class T>
  struct type_tag<const volatile T> : type_tag<T>
  {
  };

  template<class T>
  struct type_tag<T&>
  {
    static Type get(RuntimeContext* ctx) noexcept { return type_of<T*>(ctx); }
  };

  template<class T>
  struct type_tag<const T&>
  {
    static Type get(RuntimeContext* ctx) noexcept { return type_of<const T*>(ctx); }
  };

  template<class T>
  struct type_tag<T&&>
  {
    static Type get(RuntimeContext* ctx) noexcept { return type_of<T*>(ctx); }
  };

  template<class T>
  struct type_tag<T*>
  {
    static Type get(RuntimeContext* ctx) noexcept
    {
      if constexpr (std::is_void_v<T>)
        return type_of<void*>(ctx);
      else
      {
        if (auto ret = rt_type_create_ptr(ctx, type_of<T>(ctx), false);
            ret.result == TypeResult::TYPE_SUCCESS)
          return ret.success.type;
        return nullptr;
      }
    }
  };

  template<class T>
  struct type_tag<const T*>
  {
    static Type get(RuntimeContext* ctx) noexcept
    {
      if constexpr (std::is_void_v<T>)
        return type_of<const void*>(ctx);
      else
      {
        if (auto ret = rt_type_create_ptr(ctx, type_of<T>(ctx), true);
            ret.result == TypeResult::TYPE_SUCCESS)
          return ret.success.type;
        return nullptr;
      }
    }
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
    if (auto _ret = rt_type_create_fn(ctx, ret, {argv, sizeof...(A)});
        _ret.result == TypeResult::TYPE_SUCCESS)
      return _ret.success.type;
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
      m.name              = {nm.data(), nm.size()};
      m.description       = {doc.data(), doc.size()};
      m.address_or_offset = (uintptr_t)Offset;
      m.type              = type_of<FieldT>(ctx);
      return m;
    }
  };

  template<typename D>
  concept MemberDescriptor = requires(const D& d, RuntimeContext* ctx) {
    { d.make_member(ctx) } noexcept -> std::same_as<Member>;
  };

  template<typename T, MemberDescriptor... Ts>
  TypeResult bind_type(
      RuntimeContext* ctx, std::u8string_view name, Ts&&... ts) noexcept
  {
    Member members[sizeof...(Ts)]{};
    std::size_t i = 0;
    ((members[i++] = ts.make_member(ctx)), ...);
    auto t = rt_type_create(
        ctx, {name.data(), name.size()}, {members, sizeof...(Ts)}, alignof(T),
        sizeof(T));
    if (t.result == TypeResult::TYPE_SUCCESS)
    {
      // TODO: check result
      (void)rt_register_set_type(
          ctx, unique_opaque_type_id_for<std::remove_cv_t<T>>(), t.success.type);
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
