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
 * This header is suitable to be consumed by C compilers.
 * 
 * @author Raphael Dib Nehme
 * @date   December 2025
 *********************************************************************/
#ifndef __HG_STDCOLT_EXT_RUNTIME_TYPE_RUNTIME_TYPE
#define __HG_STDCOLT_EXT_RUNTIME_TYPE_RUNTIME_TYPE

#include <stdint.h>
#include <stdbool.h>
#include <stdcolt_runtime_type_export.h>
#include <stdcolt_runtime_type/perfect_hash_function.h>
#include <stdcolt_runtime_type/allocator.h>

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

  /// @brief Opaque struct managing the lifetimes of types
  struct stdcolt_ext_rt_RuntimeContext;
  /// @brief Opaque struct representing the VTable of named types
  struct stdcolt_ext_rt_NamedTypeVTable;
  // forward declaration
  struct stdcolt_ext_rt_TypeDesc;

  typedef struct stdcolt_ext_rt_RuntimeContext stdcolt_ext_rt_RuntimeContext;
  typedef struct stdcolt_ext_rt_NamedTypeVTable stdcolt_ext_rt_NamedTypeVTable;
  typedef struct stdcolt_ext_rt_TypeDesc stdcolt_ext_rt_TypeDesc;

  /// @brief Move function, receives `type`, `out`, `to_move`.
  /// If null, the `is_trivially_movable` bit marks the type
  /// as movable or not. A type marked trivially movable has its bits
  /// copied (using `memcpy` or such).
  typedef void (*stdcolt_ext_rt_move_fn_t)(
      const stdcolt_ext_rt_TypeDesc*, void*, void*);
  /// @brief Copy function, receives `type`, `out`, `to_copy`.
  /// If null, the `is_trivially_copyable` bit marks the type
  /// as copyable or not. A type marked trivially copyable has its bits
  /// copied (using `memcpy` or such).
  typedef bool (*stdcolt_ext_rt_copy_fn_t)(
      const stdcolt_ext_rt_TypeDesc*, void*, const void*);
  /// @brief Destroy function, receives `type`, `to_destroy`.
  /// If null, then the type is assumed to have a trivial destructor,
  /// meaning that the destructor is a no-op.
  typedef void (*stdcolt_ext_rt_destroy_fn_t)(const stdcolt_ext_rt_TypeDesc*, void*);

  enum
  {
    /// @brief A named type (such as a struct/class)
    STDCOLT_EXT_RT_TYPE_KIND_NAMED,
    /// @brief A builtin type (integers, float, and void*).
    STDCOLT_EXT_RT_TYPE_KIND_BUILTIN,
    /// @brief A pointer to another type.
    STDCOLT_EXT_RT_TYPE_KIND_POINTER,
    /// @brief A fixed size homogeneous list.
    STDCOLT_EXT_RT_TYPE_KIND_ARRAY,
    /// @brief A function type
    STDCOLT_EXT_RT_TYPE_KIND_FUNCTION,
    /// @brief An exception.
    STDCOLT_EXT_RT_TYPE_KIND_EXCEPTION,

    STDCOLT_EXT_RT_TYPE_KIND_end,
  };
  /// @brief Suitable storage to store STDCOLT_EXT_RT_TYPE_KIND_*
  typedef uint8_t stdcolt_ext_rt_TypeKind;

  enum
  {
    /// @brief Boolean type
    STDCOLT_EXT_RT_BUILTIN_TYPE_BOOL,
    /// @brief Unsigned 8-bit integer
    STDCOLT_EXT_RT_BUILTIN_TYPE_U8,
    /// @brief Unsigned 16-bit integer
    STDCOLT_EXT_RT_BUILTIN_TYPE_U16,
    /// @brief Unsigned 32-bit integer
    STDCOLT_EXT_RT_BUILTIN_TYPE_U32,
    /// @brief Unsigned 64-bit integer
    STDCOLT_EXT_RT_BUILTIN_TYPE_U64,
    /// @brief Signed 8-bit integer
    STDCOLT_EXT_RT_BUILTIN_TYPE_I8,
    /// @brief Signed 16-bit integer
    STDCOLT_EXT_RT_BUILTIN_TYPE_I16,
    /// @brief Signed 32-bit integer
    STDCOLT_EXT_RT_BUILTIN_TYPE_I32,
    /// @brief Signed 64-bit integer
    STDCOLT_EXT_RT_BUILTIN_TYPE_I64,
    /// @brief 32-bit floating point integer
    STDCOLT_EXT_RT_BUILTIN_TYPE_FLOAT,
    /// @brief 64-bit floating point integer
    STDCOLT_EXT_RT_BUILTIN_TYPE_DOUBLE,
    /// @brief Opaque (untyped) address (equivalent to `void*`)
    STDCOLT_EXT_RT_BUILTIN_TYPE_OPAQUE_ADDRESS,
    /// @brief Opaque (untyped) address (equivalent to `const void*`)
    STDCOLT_EXT_RT_BUILTIN_TYPE_CONST_OPAQUE_ADDRESS,

    STDCOLT_EXT_RT_BUILTIN_TYPE_end
  };
  /// @brief Suitable storage to store STDCOLT_EXT_RT_TYPE_KIND_*
  typedef uint8_t stdcolt_ext_rt_BuiltInType;

  /// @brief Type descriptor (owned by RuntimeContext)
  typedef struct stdcolt_ext_rt_TypeDesc
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
    stdcolt_ext_rt_RuntimeContext* owner;

    union
    {
      /// @brief Named type info (kind == KIND_NAMED)
      struct
      {
        /// @brief The move function
        stdcolt_ext_rt_move_fn_t move_fn;
        /// @brief The copy function
        stdcolt_ext_rt_copy_fn_t copy_fn;
        /// @brief The destroy function
        stdcolt_ext_rt_destroy_fn_t destroy_fn;
        /// @brief Opaque pointer to vtable for named types
        const stdcolt_ext_rt_NamedTypeVTable* vtable;
      } kind_named;

      /// @brief Builtin type info (kind == KIND_BUILTIN)
      struct
      {
        /// @brief The builtin type
        stdcolt_ext_rt_BuiltInType type;
      } kind_builtin;

      /// @brief Pointer type info (kind == KIND_POINTER)
      struct
      {
        /// @brief The type pointed to
        const stdcolt_ext_rt_TypeDesc* pointee_type;
        /// @brief True if pointee is const
        uint64_t pointee_is_const : 1;
      } kind_pointer;

      /// @brief Array type info (kind == KIND_ARRAY)
      struct
      {
        /// @brief The array type
        const stdcolt_ext_rt_TypeDesc* array_type;
        /// @brief Size of the array
        uint64_t size;
      } kind_array;

      /// @brief Function type info (kind == KIND_FUNCTION)
      struct
      {
        /// @brief The return type of the function or nullptr for void
        const stdcolt_ext_rt_TypeDesc* return_type;
        /// @brief The number of arguments of the function
        uint64_t argument_count;
        /// @brief The argument types
        const stdcolt_ext_rt_TypeDesc** argument_types;
      } kind_function;
    } info;

    // "cold" members, to keep at the end vvv

    /// @brief Extra data used internally
    void* opaque1;
    /// @brief Extra data used internally
    void* opaque2;
  } stdcolt_ext_rt_TypeDesc;

  /// @brief Shorthand for pointer to `TypeDesc`, pass by value.
  typedef const stdcolt_ext_rt_TypeDesc* stdcolt_ext_rt_Type;

  /// @brief View over an array of characters
  typedef struct
  {
    /// @brief Pointer to the array
    const char* data;
    /// @brief Size of the view
    uint64_t size;
  } stdcolt_ext_rt_StringView;

  /// @brief A member, used for `rt_type_create`.
  /// The difference between `Member` and `MemberInfo` is that
  /// a `Member` is realized: it has an offset/address.
  typedef struct
  {
    /// @brief The name of the member
    stdcolt_ext_rt_StringView name;
    /// @brief The description of the member
    stdcolt_ext_rt_StringView description;
    /// @brief The type of the member
    stdcolt_ext_rt_Type type;
    /// @brief The function address or offset to the member
    uintptr_t address_or_offset;
  } stdcolt_ext_rt_Member;

  /// @brief A member information, used for `rt_type_create_runtime`.
  /// The difference between `Member` and `MemberInfo` is that
  /// a `MemberInfo` is not realized: it does not have an offset/address.
  typedef struct
  {
    /// @brief The name of the member
    stdcolt_ext_rt_StringView name;
    /// @brief The description of the member
    stdcolt_ext_rt_StringView description;
    /// @brief The type of the member
    stdcolt_ext_rt_Type type;
  } stdcolt_ext_rt_MemberInfo;

  /// @brief View over an array of `stdcolt_ext_rt_Member`
  typedef struct
  {
    /// @brief Pointer to the array
    const stdcolt_ext_rt_Member* data;
    /// @brief Size of the view
    uint64_t size;
  } stdcolt_ext_rt_MemberView;

  /// @brief View over an array of `stdcolt_ext_rt_MemberInfo`
  typedef struct
  {
    /// @brief Pointer to the array
    const stdcolt_ext_rt_MemberInfo* data;
    /// @brief Size of the view
    uint64_t size;
  } stdcolt_ext_rt_MemberInfoView;

  /// @brief View over an array of `stdcolt_ext_rt_Type`
  typedef struct
  {
    /// @brief Pointer to the array
    const stdcolt_ext_rt_Type* data;
    /// @brief Size of the view
    uint64_t size;
  } stdcolt_ext_rt_TypeView;

  enum
  {
    /// @brief Keep the layout in the order of declaration
    STDCOLT_EXT_RT_LAYOUT_AS_DECLARED,
    /// @brief Try to minimize total size using a fast heuristic.
    /// Produces near-optimal results in practice but is not guaranteed minimal.
    /// Current implementation is O(n^2).
    STDCOLT_EXT_RT_LAYOUT_OPTIMIZE_SIZE_FAST,

    STDCOLT_EXT_RT_LAYOUT_end,
  };
  /// @brief The runtime layout for `rt_type_create_runtime`.
  typedef uint8_t stdcolt_ext_rt_Layout;

  /// @brief The result of the creation
  enum
  {
    /// @brief Successfully created the `RuntimeContext`
    STDCOLT_EXT_RT_CTX_SUCCESS,

    // parameter checks vvv

    /// @brief Invalid allocator recipe received
    STDCOLT_EXT_RT_CTX_INVALID_ALLOCATOR,
    /// @brief Invalid perfect hash function recipe received
    STDCOLT_EXT_RT_CTX_INVALID_PHF,

    // non-parameter checks, done after parameter checks vvv

    /// @brief Could not allocate necessary memory
    STDCOLT_EXT_RT_CTX_FAIL_MEMORY,
    /// @brief Could not bootstrap allocator
    STDCOLT_EXT_RT_CTX_FAIL_CREATE_ALLOCATOR,
  };
  /// @brief The result of the type creation
  typedef uint8_t stdcolt_ext_rt_ResultRuntimeContextKind;

  /// @brief Result of the creation of a `RuntimeContext`
  typedef struct
  {
    /// @brief The result of `rt_create`
    stdcolt_ext_rt_ResultRuntimeContextKind result;
    union
    {
      /// @brief Only active if `result == STDCOLT_EXT_RT_CTX_SUCCESS`.
      struct
      {
        /// @brief The context
        stdcolt_ext_rt_RuntimeContext* context;
      } success;

      /// @brief Only active if `result == STDCOLT_EXT_RT_CTX_FAIL_CREATE_ALLOCATOR`.
      struct
      {
        /// @brief Error code returned by the allocator, non-zero
        int32_t code;
      } fail_create_allocator;
    } data;
  } stdcolt_ext_rt_ResultRuntimeContext;

  enum
  {
    /// @brief Successfully created the type
    STDCOLT_EXT_RT_TYPE_SUCCESS,

    // parameter checks vvv

    /// @brief Received an invalid context (nullptr)
    STDCOLT_EXT_RT_TYPE_INVALID_CONTEXT,
    /// @brief Invalid allocator recipe received (named types)
    STDCOLT_EXT_RT_TYPE_INVALID_ALLOCATOR,
    /// @brief Invalid perfect hash function recipe received (named types)
    STDCOLT_EXT_RT_TYPE_INVALID_PHF,
    /// @brief Invalid owner received (owner did not match context)
    STDCOLT_EXT_RT_TYPE_INVALID_OWNER,
    /// @brief Invalid alignment received
    STDCOLT_EXT_RT_TYPE_INVALID_ALIGN,
    /// @brief Invalid parameter received, usually nullptr.
    /// This may also be caused by a different header/compiled version
    /// mismatch. Always verify ABI versions!
    STDCOLT_EXT_RT_TYPE_INVALID_PARAM,

    // non-parameter checks, done after parameter checks vvv

    /// @brief The name of the type is already in use (named types)
    STDCOLT_EXT_RT_TYPE_FAIL_EXISTS,
    /// @brief Could not allocate necessary memory
    STDCOLT_EXT_RT_TYPE_FAIL_MEMORY,
    /// @brief Could not bootstrap allocator (named types)
    STDCOLT_EXT_RT_TYPE_FAIL_CREATE_ALLOCATOR,
    /// @brief Could not bootstrap perfect hash function (named types)
    STDCOLT_EXT_RT_TYPE_FAIL_CREATE_PHF,
  };
  /// @brief The result of the type creation
  typedef uint8_t stdcolt_ext_rt_ResultTypeKind;

  /// @brief The result of the type creation
  typedef struct
  {
    /// @brief The result of the type creation
    stdcolt_ext_rt_ResultTypeKind result;

    union
    {
      /// @brief Only active if `result == STDCOLT_EXT_RT_TYPE_SUCCESS`.
      struct
      {
        stdcolt_ext_rt_Type type;
      } success;

      /// @brief Only active if `result == STDCOLT_EXT_RT_TYPE_FAIL_EXISTS`.
      struct
      {
        /// @brief The existing type
        stdcolt_ext_rt_Type existing_type;
      } fail_exists;

      /// @brief Only active if `result == STDCOLT_EXT_RT_TYPE_FAIL_CREATE_ALLOCATOR`.
      struct
      {
        /// @brief Error code returned by the allocator, non-zero
        int32_t code;
      } fail_create_allocator;

      /// @brief Only active if `result == STDCOLT_EXT_RT_TYPE_FAIL_CREATE_PHF`.
      struct
      {
        /// @brief Error code returned by the phf, non-zero
        int32_t code;
      } fail_create_phf;
    } data;
  } stdcolt_ext_rt_ResultType;

  enum
  {
    /// @brief The lookup was successful: address_or_offset is valid.
    /// If the requested member was a function, the address may be
    /// cast to a function pointer. Else, the offset should be
    /// added to the base address of an object to obtain a valid pointer
    /// to the field.
    STDCOLT_EXT_RT_LOOKUP_FOUND,
    /// @brief The lookup was not successful, the member did not have the type requested.
    /// The name of the member exist but has another type. The type is stored
    /// in `mismatch_type`.
    STDCOLT_EXT_RT_LOOKUP_MISMATCH_TYPE,
    /// @brief The lookup was not successful, the type in which to lookup is not named.
    STDCOLT_EXT_RT_LOOKUP_EXPECTED_NAMED,
    /// @brief The lookup was not successful, the type does not provide such a member.
    STDCOLT_EXT_RT_LOOKUP_NOT_FOUND,
  };
  /// @brief The result of the lookup
  typedef uint8_t stdcolt_ext_rt_ResultLookupKind;

  /// @brief The result of the lookup
  typedef struct
  {
    /// @brief The result of the lookup
    stdcolt_ext_rt_ResultLookupKind result;

    union
    {
      /// @brief Only active if `result == STDCOLT_EXT_RT_LOOKUP_FOUND`.
      struct
      {
        /// @brief The address of the function, or offset to the field
        uintptr_t address_or_offset;
      } found;

      /// @brief Only active if `result == STDCOLT_EXT_RT_MISMATCH_TYPE`.
      struct
      {
        /// @brief The actual type of the member
        stdcolt_ext_rt_Type actual_type;
      } mismatch_type;
    } data;
  } stdcolt_ext_rt_ResultLookup;

  /// @brief Creates a RuntimeContext.
  /// @param alloc The allocator to use for all VTable allocations
  /// @param phf The default perfect hash function builder to use for named types
  /// @return RuntimeContextResult.
  /// To prevent memory leaks, use `rt_destroy` on the resulting non-null context.
  STDCOLT_RUNTIME_TYPE_EXPORT
  stdcolt_ext_rt_ResultRuntimeContext stdcolt_ext_rt_create(
      const stdcolt_ext_rt_RecipeAllocator* alloc,
      const stdcolt_ext_rt_RecipePerfectHashFunction* phf);

  /// @brief Destroys all resources associated with a `RuntimeContext`.
  /// @warning Any usage of the context afterwards causes UB!
  /// @param ctx The context (or nullptr)
  STDCOLT_RUNTIME_TYPE_EXPORT
  void stdcolt_ext_rt_destroy(stdcolt_ext_rt_RuntimeContext* ctx);

  /*****************************/
  // NAMED AND RUNTIME LAYOUT
  /*****************************/

  /// @brief Type erased functions to manage the lifetime of an object
  typedef struct
  {
    /// @brief Move function, receives `type`, `out`, `to_move`.
    /// If null, the `is_trivially_movable` bit marks the type
    /// as movable or not. A type marked trivially movable has its bits
    /// copied (using `memcpy` or such).
    stdcolt_ext_rt_move_fn_t move_fn;
    /// @brief Copy function, receives `type`, `out`, `to_copy`.
    /// If null, the `is_trivially_copyable` bit marks the type
    /// as copyable or not. A type marked trivially copyable has its bits
    /// copied (using `memcpy` or such).
    stdcolt_ext_rt_copy_fn_t copy_fn;
    /// @brief Destroy function, receives `type`, `to_destroy`.
    /// If null, then the type is assumed to have a trivial destructor,
    /// meaning that the destructor is a no-op.
    stdcolt_ext_rt_destroy_fn_t destroy_fn;
    /// @brief Only read if `move_fn` is null, marks a type as trivially movable.
    uint64_t is_trivially_movable : 1;
    /// @brief Only read if `copy_fn` is null, marks a type as trivially copyable.
    uint64_t is_trivially_copyable : 1;
  } stdcolt_ext_rt_NamedLifetime;

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
  stdcolt_ext_rt_ResultType stdcolt_ext_rt_type_create(
      stdcolt_ext_rt_RuntimeContext* ctx, const stdcolt_ext_rt_StringView* name,
      const stdcolt_ext_rt_MemberView* members, uint64_t align, uint64_t size,
      const stdcolt_ext_rt_NamedLifetime* lifetime,
      const stdcolt_ext_rt_RecipeAllocator* alloc_override,
      const stdcolt_ext_rt_RecipePerfectHashFunction* phf_override);

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
  stdcolt_ext_rt_ResultType stdcolt_ext_rt_type_create_runtime(
      stdcolt_ext_rt_RuntimeContext* ctx, const stdcolt_ext_rt_StringView* name,
      const stdcolt_ext_rt_MemberInfoView* members, stdcolt_ext_rt_Layout layout,
      const stdcolt_ext_rt_RecipeAllocator* alloc_override,
      const stdcolt_ext_rt_RecipePerfectHashFunction* phf_override);

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
  stdcolt_ext_rt_ResultLookup stdcolt_ext_rt_type_lookup_fast(
      stdcolt_ext_rt_Type type_to_lookup, const stdcolt_ext_rt_StringView* name,
      stdcolt_ext_rt_Type expected_type);

  /// @brief Does a lookup for a member.
  /// This function is guaranteed to never generate false positives:
  /// the actual name is compared, not the hashes. Use this for untrusted inputs.
  /// @param type_to_lookup The type in which to do the lookup
  /// @param name The name of the type (exact same bytes as the one used in `rt_type_create`)
  /// @param expected_type The expected type of the member
  /// @return LookupResult
  STDCOLT_RUNTIME_TYPE_EXPORT
  stdcolt_ext_rt_ResultLookup stdcolt_ext_rt_type_lookup(
      stdcolt_ext_rt_Type type_to_lookup, const stdcolt_ext_rt_StringView* name,
      stdcolt_ext_rt_Type expected_type);

  /*****************************/
  // REFLECTION
  /*****************************/

  /// @brief Opaque iterator handle
  struct stdcolt_ext_rt_ReflectIterator;
  typedef struct stdcolt_ext_rt_ReflectIterator stdcolt_ext_rt_ReflectIterator;

  /// @brief Creates an iterator to reflect on a type.
  /// If the returned iterator is not null, then it must be advanced
  /// before being read.
  /// @param type The type to reflect against
  /// @return null or a valid iterator to pass to `reflect_advance` and `reflect_read`
  STDCOLT_RUNTIME_TYPE_EXPORT
  stdcolt_ext_rt_ReflectIterator* stdcolt_ext_rt_reflect_create(
      stdcolt_ext_rt_Type type);

  /// @brief Reads from an iterator.
  /// The iterator must be advanced before reading.
  /// The returned member info's data is owned by the context/type,
  /// do not free any of the views!
  /// @param iter The iterator (not null!)
  /// @return The member info
  STDCOLT_RUNTIME_TYPE_EXPORT
  stdcolt_ext_rt_Member stdcolt_ext_rt_reflect_read(
      stdcolt_ext_rt_ReflectIterator* iter);

  /// @brief Advances the iterator.
  /// This function advances the iterators, and returns null
  /// to mark end of iteration.
  /// @param iter The iterator to advance (not null!)
  /// @return The updated iterator
  STDCOLT_RUNTIME_TYPE_EXPORT
  stdcolt_ext_rt_ReflectIterator* stdcolt_ext_rt_reflect_advance(
      stdcolt_ext_rt_ReflectIterator* iter);

  /// @brief Destroys an iterator.
  /// This function is only needed if iteration is stopped before
  /// `reflect_advance` returns null. This is a no-op if iter is null.
  /// @param iter The iterator or null
  STDCOLT_RUNTIME_TYPE_EXPORT
  void stdcolt_ext_rt_reflect_destroy(stdcolt_ext_rt_ReflectIterator* iter);

  /*****************************/
  // BUILTIN TYPES
  /*****************************/

  /// @brief Creates a builtin type
  /// @param ctx The context that owns the resulting type (not null!)
  /// @param type The built-in type kind
  /// @return built-in type
  STDCOLT_RUNTIME_TYPE_EXPORT
  stdcolt_ext_rt_ResultType stdcolt_ext_rt_type_create_builtin(
      stdcolt_ext_rt_RuntimeContext* ctx, stdcolt_ext_rt_BuiltInType type);

  /// @brief Creates a pointer to a type
  /// @param ctx The context that owns the resulting type (not null!)
  /// @param pointee The type pointed to (not null!)
  /// @param pointee_is_const If true, the pointer to const
  /// @return pointer type
  STDCOLT_RUNTIME_TYPE_EXPORT
  stdcolt_ext_rt_ResultType stdcolt_ext_rt_type_create_ptr(
      stdcolt_ext_rt_RuntimeContext* ctx, stdcolt_ext_rt_Type pointee,
      bool pointee_is_const);

  /// @brief Creates an array of a type
  /// @param ctx The context that owns the resulting type (not null!)
  /// @param type The type of the array (not null!)
  /// @param size The size of the array
  /// @return array type
  STDCOLT_RUNTIME_TYPE_EXPORT
  stdcolt_ext_rt_ResultType stdcolt_ext_rt_type_create_array(
      stdcolt_ext_rt_RuntimeContext* ctx, stdcolt_ext_rt_Type type, uint64_t size);

  /// @brief Creates a function type
  /// @param ctx The context that owns the resulting type (not null!)
  /// @param ret The return of the function or null for function that return nothing
  /// @param args The argument types of the function (none null and all owned by ctx)
  /// @return function type
  STDCOLT_RUNTIME_TYPE_EXPORT
  stdcolt_ext_rt_ResultType stdcolt_ext_rt_type_create_fn(
      stdcolt_ext_rt_RuntimeContext* ctx, stdcolt_ext_rt_Type ret,
      stdcolt_ext_rt_TypeView args);

  /*****************************/
  // OPAQUE TYPE IDs
  /*****************************/

  /// @brief An opaque type ID, that may be mapped to a `Type`
  typedef const void* stdcolt_ext_rt_OpaqueTypeID;

  /// @brief Registers a type for a specific opaque type ID
  /// @param ctx The context in which to register
  /// @param id The opaque ID, must be unique for `type`
  /// @param type The type to register
  /// @return True on success
  STDCOLT_RUNTIME_TYPE_EXPORT
  bool stdcolt_ext_rt_register_set_type(
      stdcolt_ext_rt_RuntimeContext* ctx, stdcolt_ext_rt_OpaqueTypeID id,
      stdcolt_ext_rt_Type type);

  /// @brief Returns a type previously registered
  /// @param ctx The context in which to lookup
  /// @param id The opaque ID used on registration
  /// @return The registered type or nullptr
  STDCOLT_RUNTIME_TYPE_EXPORT
  stdcolt_ext_rt_Type stdcolt_ext_rt_register_get_type(
      stdcolt_ext_rt_RuntimeContext* ctx, stdcolt_ext_rt_OpaqueTypeID id);

  /*****************************/
  // PREPARED MEMBER
  /*****************************/

  /// @brief PreparedMember, obtained through `rt_prepare_member`.
  /// A prepared member allows even faster lookups than `rt_type_lookup_fast`,
  /// with the same guarantees (so false positives are possible).
  /// When accessing the same member multiple times, either cache the obtained
  /// pointer (this is the fastest), or create and reuse a `PreparedMember`.
  /// A prepared member is valid for the lifetime of the type it was prepared for.
  typedef struct
  {
    /// @brief The type whose member to access
    stdcolt_ext_rt_Type owner;
    /// @brief Expected member type
    stdcolt_ext_rt_Type expected;
    /// @brief Index of member (internal tag, do not modify!)
    uint64_t tag1;
    /// @brief Hash of member (internal tag, do not modify!)
    uint64_t tag2;
  } stdcolt_ext_rt_PreparedMember;

  /// @brief Creates a prepared member for faster repeated accesses to the same member.
  /// A prepared member allows faster lookups: use `rt_resolve_prepared_member`.
  /// @param owner_named The type in which to do the lookups
  /// @param member_name The member name to lookup (exact same bytes as the one used in `rt_type_create`)
  /// @param expected_type The expected type of the member
  /// @return PreparedMember
  STDCOLT_RUNTIME_TYPE_EXPORT
  stdcolt_ext_rt_PreparedMember stdcolt_ext_rt_prepare_member(
      stdcolt_ext_rt_Type owner_named, const stdcolt_ext_rt_StringView* member_name,
      stdcolt_ext_rt_Type expected_type);

  /// @brief Resolves a lookup from a prepared member
  /// @param pm The prepared member, obtained from `rt_prepare_member`.
  /// @return LookupResult
  STDCOLT_RUNTIME_TYPE_EXPORT
  stdcolt_ext_rt_ResultLookup stdcolt_ext_rt_resolve_prepared_member(
      const stdcolt_ext_rt_PreparedMember* pm);

  /*****************************/
  // VALUE AND LIFETIME
  /*****************************/

  /// @brief Header of all values
  typedef struct
  {
    /// @brief Type of the value or nullptr for empty Value.
    /// Do not modify manually!
    stdcolt_ext_rt_Type type;
    /// @brief Pointer to the value, which is either inline or on the heap.
    /// This address is used to avoid branching. Do not modify manually!
    /// @pre Must never be null if type is not null.
    void* address;
  } stdcolt_ext_rt_ValueHeader;

  // TODO: handle alignment properly

  /// @brief Alignment (in bytes) of inline buffer of `Value`.
  /// @warning Modification causes an ABI break!
#define STDCOLT_EXT_RT_VALUE_SBO_ALIGN (sizeof(void*))
  /// @brief Size (in bytes) of inline buffer of `Value`.
  /// @warning Modification causes an ABI break!
#define STDCOLT_EXT_RT_VALUE_SBO_SIZE (128 - sizeof(stdcolt_ext_rt_ValueHeader))

  /// @brief Runtime value of any time
  typedef struct
  {
    /// @brief Common header for all Value, may be read without methods.
    /// Write to any of its member is UB.
    stdcolt_ext_rt_ValueHeader header;

    /// @brief Inline buffer used for Small Buffer Optimization (SBO).
    /// Accessing (read/write) is UB, only use methods!
    uint8_t inline_buffer[STDCOLT_EXT_RT_VALUE_SBO_SIZE];
  } stdcolt_ext_rt_Value;

  /// @brief The result of fallible operations on `Value`
  enum
  {
    /// @brief Success
    STDCOLT_EXT_RT_VALUE_SUCCESS,
    /// @brief Could not allocate necessary memory
    STDCOLT_EXT_RT_VALUE_FAIL_MEMORY,
    /// @brief The copy function failed
    STDCOLT_EXT_RT_VALUE_FAIL_COPY,
    /// @brief Type is not copyable
    STDCOLT_EXT_RT_VALUE_NOT_COPYABLE,

    STDCOLT_EXT_RT_VALUE_end
  };
  typedef uint8_t stdcolt_ext_rt_ResultValueKind;

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
  stdcolt_ext_rt_ResultValueKind stdcolt_ext_rt_val_construct(
      stdcolt_ext_rt_Value* out, stdcolt_ext_rt_Type type);

  /// @brief Constructs an empty `Value`.
  /// Empty values do not need to be destroyed, and have `header.type == null`.
  /// This is guaranteed to not allocate, and always succeed.
  /// @param out The value to initialize
  /// @pre No parameter may be null, else UB!
  STDCOLT_RUNTIME_TYPE_EXPORT
  void stdcolt_ext_rt_val_construct_empty(stdcolt_ext_rt_Value* out);

  /// @brief Constructs a `Value` by stealing the storage from another `Value`.
  /// The moved-from value is marked empty, as if initialized by `val_construct_empty`.
  /// @param out The value to initialize (out param)
  /// @param to_move `Value` from which to steal the storage, marked empty after the call
  STDCOLT_RUNTIME_TYPE_EXPORT
  void stdcolt_ext_rt_val_construct_from_move(
      stdcolt_ext_rt_Value* out, stdcolt_ext_rt_Value* to_move);

  /// @brief Tries to copy a `Value` to another.
  /// For a copy to be successful, the type stored in `to_copy` must support
  /// being copied, and storage allocation (if needed) should succeed.
  /// On failure, `out` is marked empty, as if initialized by `val_construct_empty`.
  /// @param out The value to copy
  /// @param to_copy `Value` to copy.
  /// @return True on success, false on failure
  /// @pre No parameter may be null, else UB!
  STDCOLT_RUNTIME_TYPE_EXPORT
  stdcolt_ext_rt_ResultValueKind stdcolt_ext_rt_val_construct_from_copy(
      stdcolt_ext_rt_Value* out, const stdcolt_ext_rt_Value* to_copy);

  /// @brief Destroys the stored value then the storage of a Value.
  /// @param val Value to destroy (or null), marked empty afterwards
  STDCOLT_RUNTIME_TYPE_EXPORT
  void stdcolt_ext_rt_val_destroy(stdcolt_ext_rt_Value* val);

#ifdef __cplusplus
}
#endif // !__cplusplus

#endif
