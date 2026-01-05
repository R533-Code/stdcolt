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
  struct RuntimeContext;
  /// @brief Opaque struct representing the VTable of named types
  struct NamedTypeVTable;
  // forward declaration
  struct TypeDesc;

  /// @brief Move function, receives `type`, `out`, `to_move`.
  /// If null, the `is_trivially_movable` bit marks the type
  /// as movable or not. A type marked trivially movable has its bits
  /// copied (using `memcpy` or such).
  using move_fn_t = void (*)(const TypeDesc*, void*, void*) noexcept;
  /// @brief Copy function, receives `type`, `out`, `to_copy`.
  /// If null, the `is_trivially_copyable` bit marks the type
  /// as copyable or not. A type marked trivially copyable has its bits
  /// copied (using `memcpy` or such).
  using copy_fn_t = bool (*)(const TypeDesc*, void*, const void*) noexcept;
  /// @brief Destroy function, receives `type`, `to_destroy`.
  /// If null, then the type is assumed to have a trivial destructor,
  /// meaning that the destructor is a no-op.
  using destroy_fn_t = void (*)(const TypeDesc*, void*) noexcept;

  /// @brief The type kind
  enum class TypeKind : uint8_t
  {
    /// @brief A named type (such as a struct/class)
    KIND_NAMED,
    /// @brief A builtin type (integers, float, and void*).
    KIND_BUILTIN,
    /// @brief A pointer to another type.
    KIND_POINTER,
    /// @brief A fixed size homogeneous list.
    KIND_ARRAY,
    /// @brief A function type
    KIND_FUNCTION,
    /// @brief An exception.
    KIND_EXCEPTION,
  };

  /// @brief A built-in type
  enum class BuiltInType : uint8_t
  {
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

  /// @brief Type descriptor (owned by RuntimeContext)
  struct TypeDesc
  {
    /// @brief The type kind
    uint64_t kind : 4;
    /// @brief The alignment of the type (in bytes)
    uint64_t type_align : 32;
    /// @brief True if the values of the type are trivially movable.
    /// Trivially movable types can have their storage copied using `memcpy`.
    uint64_t trivial_movable : 1;
    /// @brief True if the type provides a move function.
    /// This is only possible if the type is named.
    uint64_t has_move_fn : 1;
    /// @brief True if the values of the type are trivially copyable.
    /// Trivially copyable types can have their storage copied using `memcpy`.
    uint64_t trivial_copyable : 1;
    /// @brief True if the type provides a copy function.
    /// This is only possible if the type is named.
    uint64_t has_copy_fn : 1;
    /// @brief True if the values of the type are trivially destructible.
    /// Trivially destructible types do not have a destructor, thus destroy is a no-op.
    /// If the type is not trivially destroyable, it MUST provide a destroy function.
    /// This is only possible if the type is named.
    uint64_t trivial_destroy : 1;
    /// @brief Unused bits  (for now)
    uint64_t _unused : 23;
    /// @brief The size of the type (in bytes)
    uint64_t type_size;
    /// @brief The context that owns the type
    RuntimeContext* owner;

    union
    {
      /// @brief Named type info (kind == KIND_NAMED)
      struct
      {
        /// @brief The move function
        move_fn_t move_fn;
        /// @brief The copy function
        copy_fn_t copy_fn;
        /// @brief The destroy function
        destroy_fn_t destroy_fn;
        /// @brief Opaque pointer to vtable for named types
        const NamedTypeVTable* vtable;
      } kind_named;

      /// @brief Builtin type info (kind == KIND_BUILTIN)
      struct
      {
        /// @brief The builtin type
        BuiltInType type;
      } kind_builtin;

      /// @brief Pointer type info (kind == KIND_POINTER)
      struct
      {
        /// @brief The type pointed to
        const TypeDesc* pointee_type;
        /// @brief True if pointee is const
        uint64_t pointee_is_const : 1;
      } kind_pointer;

      /// @brief Array type info (kind == KIND_ARRAY)
      struct
      {
        /// @brief The array type
        const TypeDesc* array_type;
        /// @brief Size of the array
        uint64_t size;
      } kind_array;

      /// @brief Function type info (kind == KIND_FUNCTION)
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

    // "cold" members, to keep at the end vvv

    /// @brief Extra data used internally
    void* opaque1;
    /// @brief Extra data used internally
    void* opaque2;
  };

  /// @brief Shorthand for pointer to `TypeDesc`, pass by value.
  using Type = const TypeDesc*;

  /// @brief A member, used for `rt_type_create`.
  /// The difference between `Member` and `MemberInfo` is that
  /// a `Member` is realized: it has an offset/address.
  struct Member
  {
    /// @brief The name of the member
    std::span<const char8_t> name;
    /// @brief The description of the member
    std::span<const char8_t> description;
    /// @brief The type of the member
    Type type;
    /// @brief The function address or offset to the member
    uintptr_t address_or_offset;
  };

  /// @brief A member information, used for `rt_type_create_runtime`.
  /// The difference between `Member` and `MemberInfo` is that
  /// a `MemberInfo` is not realized: it does not have an offset/address.
  struct MemberInfo
  {
    /// @brief The name of the member
    std::span<const char8_t> name;
    /// @brief The description of the member
    std::span<const char8_t> description;
    /// @brief The type of the member
    Type type;
  };

  /// @brief The runtime layout for `rt_type_create_runtime`.
  enum class RuntimeTypeLayout : uint8_t
  {
    /// @brief Keep the layout in the order of declaration
    LAYOUT_AS_DECLARED,
    /// @brief Try to minimize total size using a fast heuristic.
    /// Produces near-optimal results in practice but is not guaranteed minimal.
    /// Current implementation is O(n^2).
    LAYOUT_OPTIMIZE_SIZE_FAST,

    _RuntimeTypeLayout_end,
  };

  /// @brief Result of the creation of a `RuntimeContext`
  struct RuntimeContextResult
  {
    /// @brief The result of the creation
    enum class ResultKind : uint8_t
    {
      /// @brief Successfully created the `RuntimeContext`
      RT_SUCCESS,

      // parameter checks vvv

      /// @brief Invalid allocator recipe received
      RT_INVALID_ALLOCATOR,
      /// @brief Invalid perfect hash function recipe received
      RT_INVALID_PHF,

      // non-parameter checks, done after parameter checks vvv

      /// @brief Could not allocate necessary memory
      RT_FAIL_MEMORY,
      /// @brief Could not bootstrap allocator
      RT_FAIL_CREATE_ALLOCATOR,
    };
    using enum ResultKind;

    /// @brief The result of `rt_create`
    ResultKind result;
    union
    {
      /// @brief Only active if `result == ResultKind::RT_SUCCESS`.
      struct
      {
        /// @brief The context
        RuntimeContext* context;
      } success;

      /// @brief Only active if `result == ResultKind::RT_FAIL_CREATE_ALLOCATOR`.
      struct
      {
        /// @brief Error code returned by the allocator, non-zero
        int32_t code;
      } fail_create_allocator;
    };
  };

  struct TypeResult
  {
    /// @brief The result of the type creation
    enum class ResultKind : uint8_t
    {
      /// @brief Successfully created the type
      TYPE_SUCCESS,

      // parameter checks vvv

      /// @brief Received an invalid context (nullptr)
      TYPE_INVALID_CONTEXT,
      /// @brief Invalid allocator recipe received (named types)
      TYPE_INVALID_ALLOCATOR,
      /// @brief Invalid perfect hash function recipe received (named types)
      TYPE_INVALID_PHF,
      /// @brief Invalid owner received (owner did not match context)
      TYPE_INVALID_OWNER,
      /// @brief Invalid alignment received
      TYPE_INVALID_ALIGN,
      /// @brief Invalid parameter received, usually nullptr.
      /// This may also be caused by a different header/compiled version
      /// mismatch. Always verify ABI versions!
      TYPE_INVALID_PARAM,

      // non-parameter checks, done after parameter checks vvv

      /// @brief The name of the type is already in use (named types)
      TYPE_FAIL_EXISTS,
      /// @brief Could not allocate necessary memory
      TYPE_FAIL_MEMORY,
      /// @brief Could not bootstrap allocator (named types)
      TYPE_FAIL_CREATE_ALLOCATOR,
      /// @brief Could not bootstrap perfect hash function (named types)
      TYPE_FAIL_CREATE_PHF,
    };
    using enum ResultKind;

    /// @brief The result of the type creation
    ResultKind result;

    union
    {
      /// @brief Only active if `result == ResultKind::TYPE_SUCCESS`.
      struct
      {
        Type type;
      } success;

      /// @brief Only active if `result == ResultKind::TYPE_FAIL_EXISTS`.
      struct
      {
        /// @brief The existing type
        Type existing_type;
      } fail_exists;

      /// @brief Only active if `result == ResultKind::TYPE_FAIL_CREATE_ALLOCATOR`.
      struct
      {
        /// @brief Error code returned by the allocator, non-zero
        int32_t code;
      } fail_create_allocator;

      /// @brief Only active if `result == ResultKind::TYPE_FAIL_CREATE_PHF`.
      struct
      {
        /// @brief Error code returned by the phf, non-zero
        int32_t code;
      } fail_create_phf;
    };
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
      /// @brief The lookup was not successful, the member did not have the type requested.
      /// The name of the member exist but has another type. The type is stored
      /// in `mismatch_type`.
      LOOKUP_MISMATCH_TYPE,
      /// @brief The lookup was not successful, the type in which to lookup is not named.
      LOOKUP_EXPECTED_NAMED,
      /// @brief The lookup was not successful, the type does not provide such a member.
      LOOKUP_NOT_FOUND,
    };
    using enum ResultKind;

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

  /// @brief Creates a RuntimeContext.
  /// @param alloc The allocator to use for all VTable allocations
  /// @param phf The default perfect hash function builder to use for named types
  /// @return RuntimeContextResult.
  /// To prevent memory leaks, use `rt_destroy` on the resulting non-null context.
  STDCOLT_RUNTIME_TYPE_EXPORT
  RuntimeContextResult rt_create(
      const RecipeAllocator* alloc         = nullptr,
      const RecipePerfectHashFunction* phf = nullptr) noexcept;

  /// @brief Destroys all resources associated with a `RuntimeContext`.
  /// @warning Any usage of the context afterwards causes UB!
  /// @param ctx The context (or nullptr)
  STDCOLT_RUNTIME_TYPE_EXPORT
  void rt_destroy(RuntimeContext* ctx) noexcept;

  /*****************************/
  // NAMED AND RUNTIME LAYOUT
  /*****************************/

  /// @brief Type erased functions to manage the lifetime of an object
  struct NamedLifetime
  {
    /// @brief Move function, receives `type`, `out`, `to_move`.
    /// If null, the `is_trivially_movable` bit marks the type
    /// as movable or not. A type marked trivially movable has its bits
    /// copied (using `memcpy` or such).
    move_fn_t move_fn;
    /// @brief Copy function, receives `type`, `out`, `to_copy`.
    /// If null, the `is_trivially_copyable` bit marks the type
    /// as copyable or not. A type marked trivially copyable has its bits
    /// copied (using `memcpy` or such).
    copy_fn_t copy_fn;
    /// @brief Destroy function, receives `type`, `to_destroy`.
    /// If null, then the type is assumed to have a trivial destructor,
    /// meaning that the destructor is a no-op.
    destroy_fn_t destroy_fn;
    /// @brief Only read if `move_fn` is null, marks a type as trivially movable.
    uint64_t is_trivially_movable : 1;
    /// @brief Only read if `copy_fn` is null, marks a type as trivially copyable.
    uint64_t is_trivially_copyable : 1;
  };

  /// @brief Creates a named type from members
  /// @param ctx The context that owns the resulting type (not null!)
  /// @param name The name of the type (those exact bytes should be used to do a lookup)
  /// @param members The members of the type
  /// @param align The alignment of the type
  /// @param size The size of the type
  /// @param lifetime The functions to manage the lifetime of the instances (not null!)
  /// @param alloc_override If not null, allocation of instances of this type will use that allocator.
  /// If this parameter is null, the default allocator of the RuntimeContext is used.
  /// @param phf_override If not null, the perfect hash function of the current type will use that.
  /// If this parameter is null, the default perfect hash function of the RuntimeContext is used.
  /// @return A Type or an error type on invalid parameters.
  STDCOLT_RUNTIME_TYPE_EXPORT
  TypeResult rt_type_create(
      RuntimeContext* ctx, std::span<const char8_t> name,
      std::span<const Member> members, uint64_t align, uint64_t size,
      const NamedLifetime* lifetime, const RecipeAllocator* alloc_override = nullptr,
      const RecipePerfectHashFunction* phf_override = nullptr) noexcept;

  /// @brief Creates a named type from a list of fields.
  /// This computes the offsets of the fields at runtime, then
  /// calls `rt_type_create` with those offsets.
  /// @param ctx The context that owns the resulting type (not null!)
  /// @param name The name of the type (those exact bytes should be used to do a lookup)
  /// @param members The members of the type
  /// @param layout The layout to use to compute offsets of members
  /// @param alloc_override If not null, allocation of instances of this type will use that allocator.
  /// If this parameter is null, the default allocator of the RuntimeContext is used.
  /// @param phf_override If not null, the perfect hash function of the current type will use that.
  /// If this parameter is null, the default perfect hash function of the RuntimeContext is used.
  /// @return A Type or an error type on invalid parameters.
  STDCOLT_RUNTIME_TYPE_EXPORT
  TypeResult rt_type_create_runtime(
      RuntimeContext* ctx, std::span<const char8_t> name,
      std::span<const MemberInfo> members, RuntimeTypeLayout layout,
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

  /*****************************/
  // BUILTIN TYPES
  /*****************************/

  /// @brief Creates a builtin type
  /// @param ctx The context that owns the resulting type (not null!)
  /// @param type The built-in type kind
  /// @return built-in type
  STDCOLT_RUNTIME_TYPE_EXPORT
  TypeResult rt_type_create_builtin(RuntimeContext* ctx, BuiltInType type) noexcept;

  /// @brief Creates a pointer to a type
  /// @param ctx The context that owns the resulting type (not null!)
  /// @param pointee The type pointed to (not null!)
  /// @param pointee_is_const If true, the pointer to const
  /// @return pointer type
  STDCOLT_RUNTIME_TYPE_EXPORT
  TypeResult rt_type_create_ptr(
      RuntimeContext* ctx, Type pointee, bool pointee_is_const) noexcept;

  /// @brief Creates an array of a type
  /// @param ctx The context that owns the resulting type (not null!)
  /// @param type The type of the array (not null!)
  /// @param size The size of the array
  /// @return array type
  STDCOLT_RUNTIME_TYPE_EXPORT
  TypeResult rt_type_create_array(
      RuntimeContext* ctx, Type type, uint64_t size) noexcept;

  /// @brief Creates a function type
  /// @param ctx The context that owns the resulting type (not null!)
  /// @param ret The return of the function or null for function that return nothing
  /// @param args The argument types of the function (none null and all owned by ctx)
  /// @return function type
  STDCOLT_RUNTIME_TYPE_EXPORT
  TypeResult rt_type_create_fn(
      RuntimeContext* ctx, Type ret, std::span<const Type> args) noexcept;

  /*****************************/
  // OPAQUE TYPE IDs
  /*****************************/

  /// @brief An opaque type ID, that may be mapped to a `Type`
  using OpaqueTypeID = const void*;

  /// @brief Registers a type for a specific opaque type ID
  /// @param ctx The context in which to register
  /// @param id The opaque ID, must be unique for `type`
  /// @param type The type to register
  /// @return True on success
  STDCOLT_RUNTIME_TYPE_EXPORT
  bool rt_register_set_type(
      RuntimeContext* ctx, OpaqueTypeID id, Type type) noexcept;

  /// @brief Returns a type previously registered
  /// @param ctx The context in which to lookup
  /// @param id The opaque ID used on registration
  /// @return The registered type or nullptr
  STDCOLT_RUNTIME_TYPE_EXPORT
  Type rt_register_get_type(RuntimeContext* ctx, OpaqueTypeID id) noexcept;

  /*****************************/
  // PREPARED MEMBER
  /*****************************/

  /// @brief PreparedMember, obtained through `rt_prepare_member`.
  /// A prepared member allows even faster lookups than `rt_type_lookup_fast`,
  /// with the same guarantees (so false positives are possible).
  /// When accessing the same member multiple times, either cache the obtained
  /// pointer (this is the fastest), or create and reuse a `PreparedMember`.
  /// A prepared member is valid for the lifetime of the type it was prepared for.
  struct PreparedMember
  {
    /// @brief The type whose member to access
    Type owner;
    /// @brief Expected member type
    Type expected;
    /// @brief Index of member (internal tag, do not modify!)
    uint64_t tag1;
    /// @brief Hash of member (internal tag, do not modify!)
    uint64_t tag2;
  };

  /// @brief Creates a prepared member for faster repeated accesses to the same member.
  /// A prepared member allows faster lookups: use `rt_resolve_prepared_member`.
  /// @param owner_named The type in which to do the lookups
  /// @param member_name The member name to lookup (exact same bytes as the one used in `rt_type_create`)
  /// @param expected_type The expected type of the member
  /// @return PreparedMember
  STDCOLT_RUNTIME_TYPE_EXPORT
  PreparedMember rt_prepare_member(
      Type owner_named, std::span<const char8_t> member_name,
      Type expected_type) noexcept;

  /// @brief Resolves a lookup from a prepared member
  /// @param pm The prepared member, obtained from `rt_prepare_member`.
  /// @return LookupResult
  STDCOLT_RUNTIME_TYPE_EXPORT
  LookupResult rt_resolve_prepared_member(const PreparedMember& pm) noexcept;

  /*****************************/
  // VALUE AND LIFETIME
  /*****************************/

  /// @brief Header of all values
  struct ValueHeader
  {
    /// @brief Type of the value or nullptr for empty Value.
    /// Do not modify manually!
    Type type;
    /// @brief Pointer to the value, which is either inline or on the heap.
    /// This address is used to avoid branching. Do not modify manually!
    /// @pre Must never be null of type is not null.
    void* address;
  };

  /// @brief Alignment (in bytes) of inline buffer of `Value`.
  /// @warning Modification causes an ABI break!
  static constexpr size_t VALUE_SBO_ALIGN = alignof(ValueHeader);
  /// @brief Size (in bytes) of inline buffer of `Value`.
  /// @warning Modification causes an ABI break!
  static constexpr size_t VALUE_SBO_SIZE = 128 - sizeof(ValueHeader);

  /// @brief Runtime value of any time
  struct Value
  {
    /// @brief Common header for all Value, may be read without methods.
    /// Write to any of its member is UB.
    ValueHeader header;

    /// @brief Inline buffer used for Small Buffer Optimization (SBO).
    /// Accessing (read/write) is UB, only use methods!
    alignas(VALUE_SBO_ALIGN) uint8_t inline_buffer[VALUE_SBO_SIZE];
  };

  /// @brief Initialize the storage of a `Value` for a specific type.
  /// This function does not initialize the stored value: this is not
  /// a constructor call, this is a storage allocation. Initialization
  /// of the storage must be done through `header.address` if this function
  /// returns true.
  /// @param out The value to initialize (out param)
  /// @param type The type to store in the value or null for empty
  /// @return True on success, else failure
  /// @pre `out` may not be null, else UB!
  STDCOLT_RUNTIME_TYPE_EXPORT
  bool val_construct(Value* out, Type type) noexcept;

  /// @brief Constructs an empty `Value`.
  /// Empty values do not need to be destroyed, and have `header.type == null`.
  /// This is guaranteed to not allocate, and always succeed.
  /// @param out The value to initialize
  /// @pre No parameter may be null, else UB!
  STDCOLT_RUNTIME_TYPE_EXPORT
  void val_construct_empty(Value* out) noexcept;

  /// @brief Constructs a `Value` by stealing the storage from another `Value`.
  /// The moved-from value is marked empty, as if initialized by `val_construct_empty`.
  /// @param out The value to initialize (out param)
  /// @param to_move `Value` from which to steal the storage, marked empty after the call
  STDCOLT_RUNTIME_TYPE_EXPORT
  void val_construct_from_move(Value* out, Value* to_move) noexcept;

  /// @brief Tries to copy a `Value` to another.
  /// For a copy to be successful, the type stored in `to_copy` must support
  /// being copied, and storage allocation (if needed) should succeed.
  /// On failure, `out` is marked empty, as if initialized by `val_construct_empty`.
  /// @param out The value to copy
  /// @param to_copy `Value` to copy.
  /// @return True on success, false on failure
  /// @pre No parameter may be null, else UB!
  STDCOLT_RUNTIME_TYPE_EXPORT
  bool val_construct_from_copy(Value* out, const Value* to_copy) noexcept;

  /// @brief Destroys the stored value then the storage of a Value.
  /// @param val Value to destroy (or null), marked empty afterwards
  STDCOLT_RUNTIME_TYPE_EXPORT
  void val_destroy(Value* val) noexcept;
} // namespace stdcolt::ext::rt

#endif
