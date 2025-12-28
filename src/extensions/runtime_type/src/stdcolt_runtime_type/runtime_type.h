/*****************************************************************/ /**
 * @file   runtime_type.h
 * @brief  Provides a runtime type registration mechanism.
 * Most interpreted languages provide means to create types at runtime.
 * This powerful ability comes at a cost: type information need to
 * be stored at runtime and all member/method accesses need to be checked
 * for existence at runtime.
 * In compiled languages such as `C++`, types only exist at compile-time,
 * leading to better codegen: member accesses are translated to
 * `pointer + offset`.
 * This `stdcolt` extension provides a performant runtime type
 * creation/registration framework in `C++`. The API is designed
 * to be easily compatible with `C`, and ABI is guaranteed to be stable.
 * A primary usage example is cross-languages communication.
 * 
 * This extension is specifically designed for a fixed number of members
 * per types: perfect hash functions are used for performance.
 * 
 * @author Raphael Dib Nehme
 * @date   December 2025
 *********************************************************************/
#ifndef __HG_STDCOLT_EXT_RUNTIME_TYPE_RUNTIME_TYPE
#define __HG_STDCOLT_EXT_RUNTIME_TYPE_RUNTIME_TYPE

#include <stdcolt_runtime_type_export.h>
#include <stdcolt_allocators/allocator.h>
#include <stdcolt_contracts/contracts.h>
#include <stdcolt_runtime_type/perfect_hash_function.h>
#include <stdcolt_runtime_type/allocator.h>
#include <string_view>
#include <type_traits>
#include <span>
#include <memory>
#include <cstddef>

namespace stdcolt::ext::rt
{
  /// @brief Opaque struct managing the lifetimes of types
  struct RuntimeTypeContext;
  /// @brief Opaque struct representing the VTable of named types
  struct NamedTypeVTable;

  /// @brief The type kind
  enum class TypeKind : uint8_t
  {
    /// @brief A named type (such as a struct/class)
    KIND_NAMED,
    /// @brief A builtin type (integers, float, void, and void*).
    /// `void*` (named opaque address) is a builtin type, it doesn't
    /// make sense to provide a pointer to void, we only care about the
    /// address.
    KIND_BUILTIN,
    /// @brief A pointer to another type.
    KIND_POINTER,
    /// @brief A function type
    KIND_FUNCTION,
    /// @brief An exception.
    KIND_EXCEPTION,
  };

  /// @brief A built-in type
  enum class BuiltInType : uint8_t
  {
    /// @brief Error type (represents an invalid usage of the API)
    TYPE_ERROR,
    /// @brief Boolean type
    TYPE_BOOL,
    /// @brief Unsigned 8-bit integer
    TYPE_U8,
    /// @brief Unsigned 16-bit integer
    TYPE_U16,
    /// @brief Unsigned 32-bit integer
    TYPE_U32,
    /// @brief Unsigned 64-bit integer
    TYPE_U64,
    /// @brief Signed 8-bit integer
    TYPE_I8,
    /// @brief Signed 16-bit integer
    TYPE_I16,
    /// @brief Signed 32-bit integer
    TYPE_I32,
    /// @brief Signed 64-bit integer
    TYPE_I64,
    /// @brief 32-bit floating point integer
    TYPE_FLOAT,
    /// @brief 64-bit floating point integer
    TYPE_DOUBLE,
    /// @brief Opaque (untyped) address (equivalent to `void*`)
    TYPE_OPAQUE_ADDRESS,
    /// @brief Opaque (untyped) address (equivalent to `const void*`)
    TYPE_CONST_OPAQUE_ADDRESS,
  };

  /// @brief Type descriptor (owned by RuntimeTypeContext)
  struct TypeDesc
  {
    /// @brief The type kind
    uint64_t kind : 4;
    /// @brief Unused bits (for now)
    uint64_t _unused : 60;
    /// @brief The context that owns the type
    RuntimeTypeContext* owner;
    /// @brief Extra data used internally
    void* opaque1;
    /// @brief Extra data used internally
    void* opaque2;

    union
    {
      /// @brief Named type info (kind == KIND_NAMED)
      struct
      {
        /// @brief Opaque pointer to vtable for named types
        const NamedTypeVTable* vtable;
      } kind_named;

      /// @brief Builtin type info (kind == KIND_BUILTIN)
      struct
      {
        /// @brief The builtin type
        BuiltInType type;
      } kind_builtin;

      /// @brief Builtin type info (kind == KIND_POINTER)
      struct
      {
        /// @brief The type pointed to
        const TypeDesc* pointee_type;
        /// @brief True if pointee is const
        uint64_t pointee_is_const : 1;
      } kind_pointer;

      /// @brief Builtin type info (kind == KIND_FUNCTION)
      struct
      {
        /// @brief The return type of the function or nullptr for void
        const TypeDesc* return_type;
        /// @brief The number of arguments of the function
        uint64_t argument_count;
        /// @brief The argument types
        const TypeDesc** argument_types;
      } kind_function;
    };
  };

  /// @brief Shorthand for pointer to `TypeDesc`, pass by value.
  using Type = const TypeDesc*;

  /// @brief A member
  struct Member
  {
    /// @brief The name of the member
    std::span<const char8_t> name;
    /// @brief The description of the member
    std::span<const char8_t> description;
    /// @brief The function address or offset to the member
    uintptr_t address_or_offset;
    /// @brief The type of the member
    Type type;
  };

  /// @brief Lookup result
  struct LookupResult
  {
    /// @brief The result of the lookup
    enum class ResultKind : uint8_t
    {
      /// @brief The lookup was successful: address_or_offset is valid.
      /// If the requested member was a function, the address may be
      /// cast to a function pointer. Else, the offset should be
      /// added to the base address of an object to obtain a valid pointer
      /// to the field.
      LOOKUP_FOUND,
      /// @brief The lookup was not successful, the type is not a named type.
      LOOKUP_EXPECTED_NAMED,
      /// @brief The lookup was not successful, the type does not provide such a member.
      LOOKUP_NOT_FOUND,
      /// @brief The lookup was not successful, the member did not have the type requested.
      /// The name of the member exist but has another type. The type is stored
      /// in `mismatch_type`.
      LOOKUP_MISMATCH_TYPE,
    };

    /// @brief The result of the lookup
    ResultKind result;

    union
    {
      /// @brief Only active if `result == ResultKind::LOOKUP_FOUND`.
      struct
      {
        /// @brief The address of the function, or offset to the field
        uintptr_t address_or_offset;
      } found;

      /// @brief Only active if `result == ResultKind::MISMATCH_TYPE`.
      struct
      {
        /// @brief The actual type of the member
        Type actual_type;
      } mismatch_type;
    };
  };

  /// @brief Creates a RuntimeTypeContext.
  /// @param alloc The allocator to use for all VTable allocations
  /// @param phf The default perfect hash function builder to use for named types
  /// @return nullptr on errors (invalid allocator/perfect hash function builder) or a valid RuntimeTypeContext.
  /// To prevent memory leaks, use `rt_destroy`.
  STDCOLT_RUNTIME_TYPE_EXPORT
  RuntimeTypeContext* rt_create(
      const RecipeAllocator& alloc = default_allocator(),
      const RecipePerfectHashFunction& phf =
          default_perfect_hash_function()) noexcept;

  /// @brief Destroys all resources associated with a `RuntimeTypeContext`.
  /// @warning Any usage of the context afterwards causes UB!
  /// @param ctx The context (or nullptr)
  STDCOLT_RUNTIME_TYPE_EXPORT
  void rt_destroy(RuntimeTypeContext* ctx) noexcept;

  /// @brief Creates a named type from members
  /// @param ctx The context that owns the resulting type (not null!)
  /// @param name The name of the type (those exact bytes should be used to do a lookup)
  /// @param members The members of the type
  /// @param alloc_override If not null, allocation of instances of this type will use that allocator.
  /// If this parameter is null, the default allocator of the RuntimeTypeContext is used.
  /// @param phf_override If not null, the perfect hash function of the current type will use that.
  /// If this parameter is null, the default perfect hash function of the RuntimeTypeContext is used.
  /// @return A Type or an error type on invalid parameters.
  STDCOLT_RUNTIME_TYPE_EXPORT
  Type rt_type_create(
      RuntimeTypeContext* ctx, std::span<const char8_t> name,
      std::span<const Member> members,
      const RecipeAllocator* alloc_override         = nullptr,
      const RecipePerfectHashFunction* phf_override = nullptr) noexcept;

  /// @brief Does a fast lookup for a member.
  /// Fast lookups do not verify the actual name of the member, its
  /// hash is used for equality checks. The type is always compared for equality,
  /// thus type safety is guaranteed. As hashes comparison are done, there
  /// is still a possibility of collision: a collision means that a wrong member
  /// of the same type is returned (so only false positives may be generated).
  /// For untrusted inputs always use `rt_type_lookup` to compare against
  /// member names and guarantee no false positives.
  /// @param type_to_lookup The type in which to do the lookup
  /// @param name The name of the type (exact same bytes as the one used in `rt_type_create`)
  /// @param expected_type The expected type of the member
  /// @return LookupResult
  STDCOLT_RUNTIME_TYPE_EXPORT
  LookupResult rt_type_lookup_fast(
      Type type_to_lookup, std::span<const char8_t> name,
      Type expected_type) noexcept;

  /// @brief Does a lookup for a member.
  /// This function is guaranteed to never generate false positives:
  /// the actual name is compared, not the hashes. Use this for untrusted inputs.
  /// @param type_to_lookup The type in which to do the lookup
  /// @param name The name of the type (exact same bytes as the one used in `rt_type_create`)
  /// @param expected_type The expected type of the member
  /// @return LookupResult
  STDCOLT_RUNTIME_TYPE_EXPORT
  LookupResult rt_type_lookup(
      Type type_to_lookup, std::span<const char8_t> name,
      Type expected_type) noexcept;

  /// @brief Creates a builtin type
  /// @param ctx The context that owns the resulting type (not null!)
  /// @param type The built-in type kind
  /// @return built-in type
  STDCOLT_RUNTIME_TYPE_EXPORT
  Type rt_type_create_builtin(RuntimeTypeContext* ctx, BuiltInType type) noexcept;

  /// @brief Creates a pointer to a type
  /// @param ctx The context that owns the resulting type (not null!)
  /// @param pointee The type pointed to (not null!)
  /// @param pointee_is_const If true, the pointer to const
  /// @return pointer type
  STDCOLT_RUNTIME_TYPE_EXPORT
  Type rt_type_create_ptr(
      RuntimeTypeContext* ctx, Type pointee, bool pointee_is_const) noexcept;

  /// @brief Creates a function type
  /// @param ctx The context that owns the resulting type (not null!)
  /// @param ret The return of the function or null for function that return nothing
  /// @param args The argument types of the function (none null and all owned by ctx)
  /// @return function type
  STDCOLT_RUNTIME_TYPE_EXPORT
  Type rt_type_create_fn(
      RuntimeTypeContext* ctx, Type ret, std::span<const Type> args) noexcept;

  using OpaqueTypeID = const void*;

  // TODO: improve me...
  template<class T>
  struct opaque_type_tag
  {
    static inline const char value = 0;
  };

  template<typename T>
  OpaqueTypeID unique_opaque_type_id_for() noexcept
  {
    static bool anchor;
    return &anchor;
  }

  /// @brief Registers a type for a specific opaque type ID
  /// @param ctx The context in which to register
  /// @param id The opaque ID, must be unique for `type`
  /// @param type The type to register
  /// @return True on success
  STDCOLT_RUNTIME_TYPE_EXPORT
  bool rt_register_set_type(
      RuntimeTypeContext* ctx, OpaqueTypeID id, Type type) noexcept;

  /// @brief Returns a type previously registered
  /// @param ctx The context in which to lookup
  /// @param id The opaque ID used on registration
  /// @return The registered type or nullptr
  STDCOLT_RUNTIME_TYPE_EXPORT
  Type rt_register_get_type(RuntimeTypeContext* ctx, OpaqueTypeID id) noexcept;

  /***********************/
  // TYPE TAGS
  /***********************/

  template<class T>
  struct type_tag
  {
    static Type get(RuntimeTypeContext* ctx) noexcept
    {
      return rt_register_get_type(
          ctx, unique_opaque_type_id_for<std::remove_cv_t<T>>());
    }
  };

  template<class T>
  Type type_of(RuntimeTypeContext* ctx) noexcept
  {
    return type_tag<T>::get(ctx);
  }

  template<>
  struct type_tag<void>
  {
    static Type get(RuntimeTypeContext*) noexcept { return nullptr; }
  };

  template<>
  struct type_tag<void*>
  {
    static Type get(RuntimeTypeContext* ctx) noexcept
    {
      return rt_type_create_builtin(ctx, BuiltInType::TYPE_OPAQUE_ADDRESS);
    }
  };

  template<>
  struct type_tag<const void*>
  {
    static Type get(RuntimeTypeContext* ctx) noexcept
    {
      return rt_type_create_builtin(ctx, BuiltInType::TYPE_CONST_OPAQUE_ADDRESS);
    }
  };

  template<>
  struct type_tag<bool>
  {
    static Type get(RuntimeTypeContext* ctx) noexcept
    {
      return rt_type_create_builtin(ctx, BuiltInType::TYPE_BOOL);
    }
  };

  template<>
  struct type_tag<uint8_t>
  {
    static Type get(RuntimeTypeContext* ctx) noexcept
    {
      return rt_type_create_builtin(ctx, BuiltInType::TYPE_U8);
    }
  };
  template<>
  struct type_tag<uint16_t>
  {
    static Type get(RuntimeTypeContext* ctx) noexcept
    {
      return rt_type_create_builtin(ctx, BuiltInType::TYPE_U16);
    }
  };
  template<>
  struct type_tag<uint32_t>
  {
    static Type get(RuntimeTypeContext* ctx) noexcept
    {
      return rt_type_create_builtin(ctx, BuiltInType::TYPE_U32);
    }
  };
  template<>
  struct type_tag<uint64_t>
  {
    static Type get(RuntimeTypeContext* ctx) noexcept
    {
      return rt_type_create_builtin(ctx, BuiltInType::TYPE_U64);
    }
  };
  template<>
  struct type_tag<int8_t>
  {
    static Type get(RuntimeTypeContext* ctx) noexcept
    {
      return rt_type_create_builtin(ctx, BuiltInType::TYPE_I8);
    }
  };
  template<>
  struct type_tag<int16_t>
  {
    static Type get(RuntimeTypeContext* ctx) noexcept
    {
      return rt_type_create_builtin(ctx, BuiltInType::TYPE_I16);
    }
  };
  template<>
  struct type_tag<int32_t>
  {
    static Type get(RuntimeTypeContext* ctx) noexcept
    {
      return rt_type_create_builtin(ctx, BuiltInType::TYPE_I32);
    }
  };
  template<>
  struct type_tag<int64_t>
  {
    static Type get(RuntimeTypeContext* ctx) noexcept
    {
      return rt_type_create_builtin(ctx, BuiltInType::TYPE_I64);
    }
  };
  template<>
  struct type_tag<float>
  {
    static Type get(RuntimeTypeContext* ctx) noexcept
    {
      return rt_type_create_builtin(ctx, BuiltInType::TYPE_FLOAT);
    }
  };
  template<>
  struct type_tag<double>
  {
    static Type get(RuntimeTypeContext* ctx) noexcept
    {
      return rt_type_create_builtin(ctx, BuiltInType::TYPE_DOUBLE);
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
    static Type get(RuntimeTypeContext* ctx) noexcept { return type_of<T*>(ctx); }
  };

  template<class T>
  struct type_tag<const T&>
  {
    static Type get(RuntimeTypeContext* ctx) noexcept
    {
      return type_of<const T*>(ctx);
    }
  };

  template<class T>
  struct type_tag<T&&>
  {
    static Type get(RuntimeTypeContext* ctx) noexcept { return type_of<T*>(ctx); }
  };

  template<class T>
  struct type_tag<T*>
  {
    static Type get(RuntimeTypeContext* ctx) noexcept
    {
      if constexpr (std::is_void_v<T>)
        return type_of<void*>(ctx);
      else
        return rt_type_create_ptr(ctx, type_of<T>(ctx), false);
    }
  };

  template<class T>
  struct type_tag<const T*>
  {
    static Type get(RuntimeTypeContext* ctx) noexcept
    {
      if constexpr (std::is_void_v<T>)
        return type_of<void*>(ctx);
      else
        return rt_type_create_ptr(ctx, type_of<T>(ctx), true);
    }
  };

  template<class R, class... A>
  struct type_tag<R (*)(A...)>
  {
    static Type get(RuntimeTypeContext* ctx) noexcept;
  };

  template<class R, class... A>
  Type type_of_fn(RuntimeTypeContext* ctx) noexcept
  {
    Type ret    = type_of<R>(ctx);
    Type argv[] = {type_of<A>(ctx)...};
    return rt_type_create_fn(ctx, ret, {argv, sizeof...(A)});
  }

  template<class R, class... A>
  Type type_tag<R (*)(A...)>::get(RuntimeTypeContext* ctx) noexcept
  {
    return type_of_fn<R, A...>(ctx);
  }

  /***********************/
  // ABI STABILITY
  /***********************/

  template<class T>
  struct abi_param
  {
    using type = T;
  };

  template<class T>
  struct abi_param<T&>
  {
    using type = T*;
  };

  template<class T>
  struct abi_param<const T&>
  {
    using type = const T*;
  };

  template<class T>
  struct abi_param<T&&>
  {
    using type = T*;
  };

  template<class T>
  using abi_param_t = typename abi_param<T>::type;

  template<class T>
  decltype(auto) from_abi(abi_param_t<T> v) noexcept
  {
    if constexpr (std::is_lvalue_reference_v<T>)
      return *v;
    else if constexpr (std::is_rvalue_reference_v<T>)
      return std::move(*v);
    else
      return v;
  }

  template<class R>
  struct abi_ret
  {
    using type = R;
  };

  template<class R>
  struct abi_ret<R&>
  {
    using type = R*;
  };

  template<class R>
  struct abi_ret<const R&>
  {
    using type = const R*;
  };

  template<class R>
  using abi_ret_t = typename abi_ret<R>::type;

  template<class R>
  abi_ret_t<R> to_abi_ret(R&& r) noexcept
  {
    if constexpr (std::is_lvalue_reference_v<R> || std::is_rvalue_reference_v<R>)
      return std::addressof(r);
    else
      return static_cast<R&&>(r);
  }

  /***********************/
  // C++ BINDINGS
  /***********************/

  template<auto PMF>
  struct method_thunk;

  // non-const method
  template<class C, class R, class... A, R (C::*PMF)(A...)>
  struct method_thunk<PMF>
  {
    using Fn = abi_ret_t<R> (*)(void*, abi_param_t<A>...);

    static abi_ret_t<R> call(void* self, abi_param_t<A>... a) noexcept(
        noexcept((((C*)self)->*PMF)(from_abi<A>(a)...)))
    {
      auto&& r = (((C*)self)->*PMF)(from_abi<A>(a)...);
      return to_abi_ret<R>(static_cast<R&&>(r));
    }

    static Type signature(RuntimeTypeContext* ctx) noexcept
    {
      return type_of_fn<abi_ret_t<R>, void*, abi_param_t<A>...>(ctx);
    }
  };

  template<class C, class R, class... A, R (C::*PMF)(A...) const>
  struct method_thunk<PMF>
  {
    using Fn = abi_ret_t<R> (*)(const void*, abi_param_t<A>...);

    static abi_ret_t<R> call(const void* self, abi_param_t<A>... a) noexcept(
        noexcept((((const C*)self)->*PMF)(from_abi<A>(a)...)))
    {
      auto&& r = (((const C*)self)->*PMF)(from_abi<A>(a)...);
      return to_abi_ret<R>(static_cast<R&&>(r));
    }

    static Type signature(RuntimeTypeContext* ctx) noexcept
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

    Member make_member(RuntimeTypeContext* ctx) const noexcept
    {
      Member m{};
      m.name              = {nm.data(), nm.size()};
      m.description       = {doc.data(), doc.size()};
      m.address_or_offset = (uintptr_t)(&method_thunk<PMF>::call);
      m.type              = method_thunk<PMF>::signature(ctx);
      return m;
    }
  };

  template<class Owner, class FieldT, std::size_t Offset>
  struct field
  {
    std::u8string_view nm;
    std::u8string_view doc{};

    constexpr field(std::u8string_view n, std::u8string_view d = {})
        : nm(n)
        , doc(d)
    {
    }

    Member make_member(RuntimeTypeContext* ctx) const noexcept
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

  template<class D>
  concept MemberDescriptor = requires(const D& d, RuntimeTypeContext* ctx) {
    { d.make_member(ctx) } noexcept -> std::same_as<Member>;
  };

  template<typename T, MemberDescriptor... Ts>
  Type bind_type(
      RuntimeTypeContext* ctx, std::u8string_view name, Ts&&... ts) noexcept
  {
    Member members[sizeof...(Ts)]{};
    std::size_t i = 0;
    ((members[i++] = ts.make_member(ctx)), ...);
    auto t =
        rt_type_create(ctx, {name.data(), name.size()}, {members, sizeof...(Ts)});
    if (t && t->kind != (uint64_t)TypeKind::KIND_BUILTIN)
      (void)rt_register_set_type(
          ctx, unique_opaque_type_id_for<std::remove_cv_t<T>>(), t);
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

#endif
