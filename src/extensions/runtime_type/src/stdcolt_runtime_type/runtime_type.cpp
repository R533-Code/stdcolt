#include "runtime_type.h"

#include <atomic>
#include <cstring>
#include <new>
#include <string>
#include <array>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <stdcolt_allocators/allocators/mallocator.h>

namespace stdcolt::ext::rt
{
  static void noop_destruct(void*) noexcept
  {
    // does nothing
  }

  static constexpr size_t BUILTIN_COUNT =
      (size_t)BuiltInType::TYPE_CONST_OPAQUE_ADDRESS + 1;

  static inline bool is_pow2(size_t n) noexcept
  {
    return n != 0 && (n & (n - 1)) == 0;
  }

  static inline size_t align_up_dyn(size_t v, size_t align) noexcept
  {
    // caller guarantees power of two and align >= 1
    return (v + (align - 1)) & ~(align - 1);
  }

  static inline bool phf_recipe_valid(const RecipePerfectHashFunction& r) noexcept
  {
    return r.phf_alignof >= 1 && r.phf_sizeof >= 1 && is_pow2(r.phf_alignof)
           && r.phf_construct != nullptr && r.phf_destruct != nullptr
           && r.phf_lookup != nullptr;
  }

  static inline bool alloc_recipe_valid(const RecipeAllocator& r) noexcept
  {
    return r.allocator_alignof >= 1 && is_pow2(r.allocator_alignof)
           && r.allocator_construct != nullptr && r.allocator_destruct != nullptr
           && r.allocator_alloc != nullptr && r.allocator_dealloc != nullptr;
  }

  static inline std::u8string to_u8string(std::span<const char8_t> s)
  {
    return std::u8string(s.data(), s.size());
  }

  // SplitMix64
  static inline uint64_t mix64(uint64_t x) noexcept
  {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    x = x ^ (x >> 31);
    return x;
  }

  static inline uint64_t load_u64_unaligned(const uint8_t* p) noexcept
  {
    uint64_t v;
    std::memcpy(&v, p, sizeof(v));
    return v;
  }

  static inline uint64_t load_tail_u64(const uint8_t* p, size_t len) noexcept
  {
    // len in [0,8]
    uint64_t v = 0;
    if (len)
      std::memcpy(&v, p, len);
    return v;
  }

  static inline uint64_t hash_name(std::span<const char8_t> s) noexcept
  {
    // TODO: check me
    const size_t len = s.size();
    auto p           = (const uint8_t*)s.data();

    uint64_t h = mix64(len);

    if (len == 0)
      return h;

    if (len <= 8)
    {
      const uint64_t a = load_tail_u64(p, len);
      return mix64(h ^ mix64(a));
    }

    const uint64_t first = load_u64_unaligned(p);
    const uint64_t last  = load_u64_unaligned(p + len - 8);

    if (len <= 16)
    {
      h ^= mix64(first) ^ mix64(last);
      return mix64(h);
    }

    const uint64_t near_start = load_u64_unaligned(p + 8);
    const size_t mid_off      = (len / 2) - 4;
    const uint64_t mid        = load_u64_unaligned(p + mid_off);

    h ^= mix64(first) ^ mix64(near_start) ^ mix64(mid) ^ mix64(last);

    if (len > 32)
    {
      const uint64_t near_end = load_u64_unaligned(p + len - 16);
      h ^= mix64(near_end);
    }
    return mix64(h);
  }

  struct transparent_u8_hash
  {
    using is_transparent = void;
    using hash_type      = std::hash<std::u8string_view>;

    size_t operator()(std::u8string_view sv) const noexcept
    {
      return hash_type{}(sv);
    }
    size_t operator()(std::span<const char8_t> sv) const noexcept
    {
      return hash_type{}(std::u8string_view{sv.data(), sv.size()});
    }
    size_t operator()(const char8_t* s) const noexcept
    {
      return hash_type{}(std::u8string_view{s});
    }
    size_t operator()(const std::u8string& s) const noexcept
    {
      return hash_type{}(std::u8string_view{s});
    }
  };

  struct RTMemberDescription
  {
    uint32_t key_size;
    uint32_t hr_description_size;

    std::span<const char8_t> key() const noexcept { return {key_c_str(), key_size}; }
    std::span<const char8_t> description() const noexcept
    {
      return {description_c_str(), hr_description_size};
    }

    const char8_t* key_c_str() const noexcept
    {
      return (const char8_t*)(this) + sizeof(RTMemberDescription);
    }
    const char8_t* description_c_str() const noexcept
    {
      return key_c_str() + (size_t)key_size + 1; // +1 for NUL
    }
  };

  struct NamedTypeVTableEntry
  {
    uintptr_t address_or_offset;
    uint64_t tag; // hash(name) for fast checks
    Type type;    // exact type pointer for safety
    RTMemberDescription* true_description;
  };

  struct NamedTypeVTable
  {
    uint64_t allocation_size;

    Allocator allocator;
    PerfectHashFunction phf;

    uint64_t count_members;
    uint32_t entries_offset;
    uint32_t _padding;

    std::span<const NamedTypeVTableEntry> entries() const noexcept
    {
      return {
          (const NamedTypeVTableEntry*)((const uint8_t*)this + entries_offset),
          count_members};
    }
  };

  struct RuntimeContext
  {
    RecipeAllocator default_alloc_recipe{};
    alloc::Block default_alloc_state = {};
    RecipePerfectHashFunction default_phf_recipe{};

    std::unordered_map<
        std::u8string, TypeDesc*, transparent_u8_hash, std::equal_to<>>
        types_table;
    std::unordered_map<OpaqueTypeID, Type> registered_type_table;
    TypeDesc builtin_types[BUILTIN_COUNT]{};

    struct PtrKey
    {
      Type pointee;
      bool pointee_const;
    };

    struct ArrayKey
    {
      Type pointee;
      uint64_t size;
    };

    struct KeyHash
    {
      size_t operator()(const PtrKey& k) const noexcept
      {
        uint64_t h = mix64((uint64_t)k.pointee);
        h ^= mix64((uint64_t)k.pointee_const);
        return h;
      }

      size_t operator()(const ArrayKey& k) const noexcept
      {
        uint64_t h = mix64((uint64_t)k.pointee);
        h ^= mix64(k.size);
        return h;
      }
    };

    struct KeyEq
    {
      bool operator()(const PtrKey& a, const PtrKey& b) const noexcept
      {
        return a.pointee == b.pointee && a.pointee_const == b.pointee_const;
      }
      bool operator()(const ArrayKey& a, const ArrayKey& b) const noexcept
      {
        return a.pointee == b.pointee && a.size == b.size;
      }
    };

    std::unordered_map<PtrKey, TypeDesc*, KeyHash, KeyEq> pointer_types;
    std::unordered_map<ArrayKey, TypeDesc*, KeyHash, KeyEq> array_types;
    std::unordered_map<uint64_t, TypeDesc*> function_buckets;
    std::atomic<uint64_t> type_id_generator{1};
  };

  static inline uint32_t builtin_sizeof(BuiltInType type) noexcept
  {
    static constexpr std::array<uint32_t, BUILTIN_COUNT> SIZES = {
        sizeof(bool),     sizeof(uint8_t), sizeof(uint16_t), sizeof(uint32_t),
        sizeof(uint64_t), sizeof(int8_t),  sizeof(int16_t),  sizeof(int32_t),
        sizeof(int64_t),  sizeof(float),   sizeof(double),   sizeof(void*),
        sizeof(void*)};

    if ((size_t)type >= BUILTIN_COUNT)
      return (uint32_t)-1;
    return SIZES[(size_t)type];
  }

  static inline LookupResult lookup_expected_named() noexcept
  {
    LookupResult r{};
    r.result = LookupResult::ResultKind::LOOKUP_EXPECTED_NAMED;
    return r;
  }

  static inline LookupResult lookup_not_found() noexcept
  {
    LookupResult r{};
    r.result = LookupResult::ResultKind::LOOKUP_NOT_FOUND;
    return r;
  }

  static inline LookupResult lookup_found(uintptr_t addr) noexcept
  {
    LookupResult r{};
    r.result                  = LookupResult::ResultKind::LOOKUP_FOUND;
    r.found.address_or_offset = addr;
    return r;
  }

  static inline LookupResult lookup_mismatch(Type actual) noexcept
  {
    LookupResult r{};
    r.result                    = LookupResult::ResultKind::LOOKUP_MISMATCH_TYPE;
    r.mismatch_type.actual_type = actual;
    return r;
  }

  static inline TypeDesc* alloc_from_ctx(
      RuntimeContext* ctx, size_t bytes, size_t align) noexcept
  {
    Block blk = ctx->default_alloc_recipe.allocator_alloc(
        ctx->default_alloc_state.ptr(), bytes, align);
    if (!blk.ptr)
      return nullptr;
    return (TypeDesc*)blk.ptr;
  }

  static inline void dealloc_from_ctx(
      RuntimeContext* ctx, void* ptr, size_t bytes) noexcept
  {
    ctx->default_alloc_recipe.allocator_dealloc(
        ctx->default_alloc_state.ptr(), Block{ptr, bytes});
  }

  /*************************/
  // RuntimeContextResult
  /*************************/

  static inline RuntimeContextResult result_rc_invalid_alloc() noexcept
  {
    return RuntimeContextResult{
        .result{RuntimeContextResult::RT_INVALID_ALLOCATOR}, .success{nullptr}};
  }
  static inline RuntimeContextResult result_rc_invalid_phf() noexcept
  {
    return RuntimeContextResult{
        .result{RuntimeContextResult::RT_INVALID_PHF}, .success{nullptr}};
  }
  static inline RuntimeContextResult result_rc_fail_mem() noexcept
  {
    return RuntimeContextResult{
        .result{RuntimeContextResult::RT_FAIL_MEMORY}, .success{nullptr}};
  }

  /*************************/
  // RuntimeContext Lifetime
  /*************************/

  RuntimeContextResult rt_create(
      const RecipeAllocator* alloc, const RecipePerfectHashFunction* phf) noexcept
  {
    RecipeAllocator alloc_recipe = alloc == nullptr ? default_allocator() : *alloc;
    RecipePerfectHashFunction phf_recipe =
        phf == nullptr ? default_perfect_hash_function() : *phf;
    if (!alloc_recipe_valid(alloc_recipe))
      return result_rc_invalid_alloc();
    if (!phf_recipe_valid(phf_recipe))
      return result_rc_invalid_phf();

    auto ctx = new (std::nothrow) RuntimeContext();
    if (ctx == nullptr)
      return result_rc_fail_mem();

    ctx->default_alloc_recipe = alloc_recipe;
    ctx->default_phf_recipe   = phf_recipe;

    alloc::Block state = alloc::MallocatorAligned{}.allocate(alloc::Layout{
        alloc_recipe.allocator_sizeof, alloc_recipe.allocator_alignof});
    if (state == alloc::nullblock)
    {
      delete ctx;
      return result_rc_fail_mem();
    }
    ctx->default_alloc_state = state;

    if (alloc_recipe.allocator_construct(ctx->default_alloc_state.ptr()) != 0)
    {
      alloc::MallocatorAligned{}.deallocate(state);
      delete ctx;
      return result_rc_fail_mem();
    }

    // initialize builtin types
    for (size_t i = 0; i < BUILTIN_COUNT; ++i)
    {
      TypeDesc& td         = ctx->builtin_types[i];
      td.kind              = (uint64_t)TypeKind::KIND_BUILTIN;
      td._unused           = 0;
      td.owner             = ctx;
      td.trivial_copyable  = true;
      td.trivial_movable   = true;
      td.trivial_destroy   = true;
      td.has_move_fn       = false;
      td.has_copy_fn       = false;
      td.opaque1           = nullptr;
      td.opaque2           = nullptr;
      td.kind_builtin.type = (BuiltInType)i;
      td.type_size         = builtin_sizeof((BuiltInType)i);
      td.type_align        = td.type_size;
    }

    return RuntimeContextResult{
        .result{RuntimeContextResult::RT_SUCCESS}, .success{ctx}};
  }

  void rt_destroy(RuntimeContext* ctx) noexcept
  {
    if (!ctx)
      return;

    // destroy named types
    for (auto& kv : ctx->types_table)
    {
      TypeDesc* td = kv.second;
      STDCOLT_debug_assert(
          td != nullptr && td->kind == (uint64_t)TypeKind::KIND_NAMED,
          "corrupted types_table");
      const NamedTypeVTable* vt = td->kind_named.vtable;
      STDCOLT_debug_assert(vt != nullptr, "corrupted vtable");

      if (vt->phf.phf_destruct)
        vt->phf.phf_destruct(vt->phf.state);
      if (vt->allocator.allocator_destruct)
        vt->allocator.allocator_destruct(vt->allocator.state);

      dealloc_from_ctx(ctx, (void*)td, vt->allocation_size);
    }
    ctx->types_table.clear();

    // destroy pointer types
    for (auto& kv : ctx->pointer_types)
    {
      TypeDesc* td = kv.second;
      STDCOLT_debug_assert(td != nullptr, "corrupted pointer_types");
      dealloc_from_ctx(ctx, (void*)td, sizeof(TypeDesc));
    }
    ctx->pointer_types.clear();

    // destroy array types
    for (auto& kv : ctx->array_types)
    {
      TypeDesc* td = kv.second;
      STDCOLT_debug_assert(td != nullptr, "corrupted array_types");
      dealloc_from_ctx(ctx, (void*)td, sizeof(TypeDesc));
    }
    ctx->array_types.clear();

    // destroy function types
    for (auto& kv : ctx->function_buckets)
    {
      // walk the chain
      TypeDesc* node = kv.second;
      while (node)
      {
        TypeDesc* next = (TypeDesc*)node->opaque1;

        const uint64_t argc = node->kind_function.argument_count;
        const size_t off    = align_up_dyn(sizeof(TypeDesc), alignof(Type));
        const size_t bytes  = off + argc * sizeof(Type);

        dealloc_from_ctx(ctx, (void*)node, bytes);
        node = next;
      }
      kv.second = nullptr;
    }
    ctx->function_buckets.clear();

    ctx->default_alloc_recipe.allocator_destruct(ctx->default_alloc_state.ptr());
    alloc::MallocatorAligned{}.deallocate(ctx->default_alloc_state);
    try
    {
      delete ctx;
    }
    catch (...)
    {
      // swallow any exception
    }
  }

  /*************************/
  // TypeResult
  /*************************/

  // clang-format off
  static inline TypeResult result_type_success(Type ret) noexcept
  {
    return {.result{TypeResult::TYPE_SUCCESS}, .success{ret}};
  }
  static inline TypeResult result_type_invalid_phf() noexcept
  {
    return {.result{TypeResult::TYPE_INVALID_PHF}, .success{nullptr}};
  }
  static inline TypeResult result_type_invalid_align() noexcept
  {
    return {.result{TypeResult::TYPE_INVALID_ALIGN}, .success{nullptr}};
  }
  static inline TypeResult result_type_invalid_alloc() noexcept
  {
    return {.result{TypeResult::TYPE_INVALID_ALLOCATOR}, .success{nullptr}};
  }
  static inline TypeResult result_type_invalid_param() noexcept
  {
    return {.result{TypeResult::TYPE_INVALID_PARAM}, .success{nullptr}};
  }
  static inline TypeResult result_type_invalid_context() noexcept
  {
    return {.result{TypeResult::TYPE_INVALID_CONTEXT}, .success{nullptr}};
  }
  static inline TypeResult result_type_invalid_owner() noexcept
  {
    return {.result{TypeResult::TYPE_INVALID_OWNER}, .success{nullptr}};
  }
  static inline TypeResult result_type_fail_mem() noexcept
  {
    return {.result{TypeResult::TYPE_FAIL_MEMORY}, .success{nullptr}};
  }
  static inline TypeResult result_type_fail_create_phf(int32_t ret) noexcept
  {
    return {.result{TypeResult::TYPE_FAIL_CREATE_PHF}, .fail_create_phf{ret}};
  }
  static inline TypeResult result_type_fail_create_allocator(int32_t ret) noexcept
  {
    return {.result{TypeResult::TYPE_FAIL_CREATE_ALLOCATOR}, .fail_create_allocator{ret}};
  }
  static inline TypeResult result_type_fail_exists(Type ret) noexcept
  {
    return {.result{TypeResult::TYPE_FAIL_EXISTS}, .fail_exists{ret}};
  }
  // clang-format on

  /*************************/
  // Type creation
  /*************************/

  TypeResult rt_type_create_builtin(RuntimeContext* ctx, BuiltInType type) noexcept
  {
    if (!ctx)
      return result_type_invalid_context();
    const auto id = (size_t)type;
    if (id >= BUILTIN_COUNT)
      return result_type_invalid_param();
    return result_type_success(&ctx->builtin_types[id]);
  }

  TypeResult rt_type_create_ptr(
      RuntimeContext* ctx, Type pointee, bool pointee_is_const) noexcept
  {
    if (!ctx)
      return result_type_invalid_context();
    if (!pointee)
      return result_type_invalid_param();
    if (pointee->owner != ctx)
      return result_type_invalid_owner();

    RuntimeContext::PtrKey key{pointee, pointee_is_const};
    if (auto it = ctx->pointer_types.find(key); it != ctx->pointer_types.end())
      return result_type_success(it->second);

    TypeDesc* td = alloc_from_ctx(ctx, sizeof(TypeDesc), alignof(TypeDesc));
    if (!td)
      return result_type_fail_mem();

    td->kind             = (uint64_t)TypeKind::KIND_POINTER;
    td->_unused          = 0;
    td->owner            = ctx;
    td->trivial_copyable = true;
    td->trivial_movable  = true;
    td->trivial_destroy  = true;
    td->has_move_fn      = false;
    td->has_copy_fn      = false;
    td->opaque1          = nullptr;
    td->opaque2          = nullptr;
    td->type_size        = sizeof(void*);
    td->type_align       = alignof(void*);

    td->kind_pointer.pointee_type     = pointee;
    td->kind_pointer.pointee_is_const = (uint64_t)pointee_is_const;

    try
    {
      ctx->pointer_types.try_emplace(key, td);
    }
    catch (...)
    {
      dealloc_from_ctx(ctx, (void*)td, sizeof(TypeDesc));
      return result_type_fail_mem();
    }
    return result_type_success(td);
  }

  TypeResult rt_type_create_array(
      RuntimeContext* ctx, Type type, uint64_t size) noexcept
  {
    if (!ctx)
      return result_type_invalid_context();
    if (!type)
      return result_type_invalid_param();
    if (type->owner != ctx)
      return result_type_invalid_owner();

    RuntimeContext::ArrayKey key{type, size};
    if (auto it = ctx->array_types.find(key); it != ctx->array_types.end())
      return result_type_success(it->second);

    TypeDesc* td = alloc_from_ctx(ctx, sizeof(TypeDesc), alignof(TypeDesc));
    if (!td)
      return result_type_fail_mem();

    td->kind             = (uint64_t)TypeKind::KIND_ARRAY;
    td->trivial_copyable = type->trivial_copyable;
    td->trivial_movable  = type->trivial_movable;
    td->trivial_destroy  = type->trivial_destroy;
    td->has_move_fn      = type->has_move_fn;
    td->has_copy_fn      = type->has_copy_fn;
    td->_unused          = 0;
    td->owner            = ctx;
    td->opaque1          = nullptr;
    td->opaque2          = nullptr;
    // TODO: handle overflow
    td->type_size  = type->type_size * size;
    td->type_align = type->type_align;

    td->kind_array.array_type = type;
    td->kind_array.size       = size;

    try
    {
      ctx->array_types.try_emplace(key, td);
    }
    catch (...)
    {
      dealloc_from_ctx(ctx, (void*)td, sizeof(TypeDesc));
      return result_type_fail_mem();
    }
    return result_type_success(td);
  }

  static inline uint64_t fn_tag(Type ret, std::span<const Type> args) noexcept
  {
    uint64_t h = mix64((uintptr_t)ret) ^ mix64(args.size());
    for (size_t i = 0; i < args.size(); ++i)
      h ^= mix64((uintptr_t)args[i] + 0x9e3779b97f4a7c15ULL * (i + 1));
    return mix64(h);
  }

  TypeResult rt_type_create_fn(
      RuntimeContext* ctx, Type ret, std::span<const Type> args) noexcept
  {
    if (!ctx)
      return result_type_invalid_context();
    if (ret && ret->owner != ctx)
      return result_type_invalid_owner();
    // only check arguments: a nullptr return represents a void return
    for (size_t i = 0; i < args.size(); ++i)
    {
      if (!args[i])
        return result_type_invalid_param();
      if (args[i]->owner != ctx)
        return result_type_invalid_owner();
    }

    const uint64_t tag = fn_tag(ret, args);

    // Deduplicate by hash bucket + exact signature compare
    if (auto bit = ctx->function_buckets.find(tag);
        bit != ctx->function_buckets.end())
    {
      TypeDesc* node = bit->second;
      while (node)
      {
        // tag matches by bucket key; still collision possible -> full compare
        if (node->kind == (uint64_t)TypeKind::KIND_FUNCTION
            && node->kind_function.return_type == ret
            && node->kind_function.argument_count == args.size())
        {
          auto argv = (const Type*)node->kind_function.argument_types;
          bool ok   = true;
          for (size_t i = 0; i < args.size(); ++i)
          {
            if (argv[i] != args[i])
            {
              ok = false;
              break;
            }
          }
          if (ok)
            return result_type_success(node);
        }
        node = (TypeDesc*)node->opaque1;
      }
    }

    // allocate single block: [TypeDesc][padding][Type argv[argc]]
    const size_t argc  = args.size();
    const size_t off   = align_up_dyn(sizeof(TypeDesc), alignof(Type));
    const size_t bytes = off + argc * sizeof(Type);

    TypeDesc* td = alloc_from_ctx(ctx, bytes, alignof(TypeDesc));
    if (!td)
      return result_type_fail_mem();

    td->kind             = (uint64_t)TypeKind::KIND_FUNCTION;
    td->_unused          = 0;
    td->trivial_copyable = true;
    td->trivial_movable  = true;
    td->trivial_destroy  = true;
    td->has_move_fn      = false;
    td->has_copy_fn      = false;
    td->owner            = ctx;
    td->type_size        = sizeof(void*);
    td->type_align       = alignof(void*);

    // store chaining + tag in opaque fields
    td->opaque2 = (void*)tag;

    Type* argv = (Type*)((uint8_t*)td + off);
    for (size_t i = 0; i < argc; ++i)
      argv[i] = args[i];

    td->kind_function.return_type    = ret;
    td->kind_function.argument_count = argc;
    td->kind_function.argument_types = argv;

    // insert into bucket head (intrusive chain via opaque1)
    TypeDesc* old_head = nullptr;
    {
      try
      {
        auto ins = ctx->function_buckets.try_emplace(tag, (TypeDesc*)nullptr);
        old_head = ins.first->second;
        ins.first->second = td;
      }
      catch (...)
      {
        // TODO: undo changes
        return result_type_fail_mem();
      }
    }
    td->opaque1 = (void*)old_head;

    return result_type_success(td);
  }

  TypeResult rt_type_create(
      RuntimeContext* ctx, std::span<const char8_t> name,
      std::span<const Member> members, uint64_t align, uint64_t size,
      const NamedLifetime* lifetime, const RecipeAllocator* alloc_override,
      const RecipePerfectHashFunction* phf_override) noexcept
  {
    // TODO: handle align up size to alignment...
    if (!ctx)
      return result_type_invalid_context();

    if (!alloc_recipe_valid(ctx->default_alloc_recipe))
      return result_type_invalid_alloc();
    if (!phf_recipe_valid(ctx->default_phf_recipe))
      return result_type_invalid_phf();
    if (lifetime == nullptr)
      return result_type_invalid_param();
    if (align == 0 || !is_pow2(align))
      return result_type_invalid_align();

    for (size_t i = 0; i < members.size(); ++i)
    {
      if (!members[i].type)
        return result_type_invalid_param();
      if (members[i].type->owner != ctx)
        return result_type_invalid_owner();
    }

    const RecipeAllocator& type_alloc =
        (alloc_override != nullptr) ? *alloc_override : ctx->default_alloc_recipe;
    const bool has_alloc_override = (alloc_override != nullptr);

    const RecipePerfectHashFunction& phf_recipe =
        (phf_override != nullptr) ? *phf_override : ctx->default_phf_recipe;

    if (has_alloc_override && !alloc_recipe_valid(type_alloc))
      return result_type_invalid_alloc();
    if (!phf_recipe_valid(phf_recipe))
      return result_type_invalid_phf();

    // deduplication by name: return existing if present
    {
      auto it = ctx->types_table.find(std::u8string_view{name.data(), name.size()});
      if (it != ctx->types_table.end() && it->second)
        return result_type_fail_exists(it->second);
    }

    const size_t n = members.size();
    // allocate TypeDesc + aligned vtable blob; offsets inside blob are relative to vtable base
    const size_t vtable_off =
        align_up_dyn(sizeof(TypeDesc), alignof(NamedTypeVTable));

    size_t cursor = 0;
    cursor += sizeof(NamedTypeVTable);

    cursor                    = align_up_dyn(cursor, alignof(NamedTypeVTableEntry));
    const auto entries_offset = (uint32_t)cursor;
    cursor += sizeof(NamedTypeVTableEntry) * n;

    size_t alloc_state_off = 0;
    if (has_alloc_override)
    {
      cursor          = align_up_dyn(cursor, (size_t)type_alloc.allocator_alignof);
      alloc_state_off = cursor;
      cursor += type_alloc.allocator_sizeof;
    }

    cursor = align_up_dyn(cursor, (size_t)phf_recipe.phf_alignof);
    const size_t phf_state_off = cursor;
    cursor += phf_recipe.phf_sizeof;

    cursor = align_up_dyn(cursor, alignof(RTMemberDescription));
    const size_t member_desc_start = cursor;

    for (const Member& m : members)
    {
      cursor = align_up_dyn(cursor, alignof(RTMemberDescription));
      cursor += sizeof(RTMemberDescription);
      // +1 for NUL terminators, for C simplicity
      cursor += m.name.size() + 1;
      cursor += m.description.size() + 1;
    }

    const size_t vtable_blob_size = cursor;
    const size_t total_size       = vtable_off + vtable_blob_size;

    size_t total_align = alloc::PREFERRED_ALIGNMENT;
    if (total_align < alignof(TypeDesc))
      total_align = alignof(TypeDesc);
    if (total_align < alignof(NamedTypeVTable))
      total_align = alignof(NamedTypeVTable);
    if (total_align < alignof(NamedTypeVTableEntry))
      total_align = alignof(NamedTypeVTableEntry);
    if (total_align < alignof(RTMemberDescription))
      total_align = alignof(RTMemberDescription);
    if (total_align < (size_t)phf_recipe.phf_alignof)
      total_align = (size_t)phf_recipe.phf_alignof;
    if (has_alloc_override && total_align < (size_t)type_alloc.allocator_alignof)
      total_align = (size_t)type_alloc.allocator_alignof;

    TypeDesc* td = alloc_from_ctx(ctx, total_size, total_align);
    if (!td)
      return result_type_fail_mem();

    auto base_all = (uint8_t*)td;
    auto vtbase   = base_all + vtable_off;
    auto* vt      = (NamedTypeVTable*)vtbase;

    td->kind    = (uint64_t)TypeKind::KIND_NAMED;
    td->_unused = 0;
    td->owner   = ctx;
    td->trivial_copyable =
        lifetime->copy_fn == nullptr && (bool)lifetime->is_trivially_copyable;
    td->trivial_movable =
        lifetime->move_fn == nullptr && (bool)lifetime->is_trivially_movable;
    td->trivial_destroy       = lifetime->destroy_fn == nullptr;
    td->has_move_fn           = lifetime->move_fn != nullptr;
    td->has_copy_fn           = lifetime->copy_fn != nullptr;
    td->type_size             = size;
    td->type_align            = align;
    td->opaque1               = nullptr;
    td->opaque2               = nullptr;
    td->kind_named.vtable     = vt;
    td->kind_named.copy_fn    = lifetime->copy_fn;
    td->kind_named.move_fn    = lifetime->move_fn;
    td->kind_named.destroy_fn = lifetime->destroy_fn;

    // vtable header
    vt->allocation_size = total_size;
    vt->count_members   = n;
    vt->entries_offset  = entries_offset;
    vt->_padding        = 0;

    // allocator state used for instances of this type
    void* type_alloc_state = has_alloc_override ? (void*)(vtbase + alloc_state_off)
                                                : ctx->default_alloc_state.ptr();

    vt->allocator.state              = type_alloc_state;
    vt->allocator.allocator_alloc    = type_alloc.allocator_alloc;
    vt->allocator.allocator_dealloc  = type_alloc.allocator_dealloc;
    vt->allocator.allocator_destruct = &noop_destruct;

    vt->phf.state        = (void*)(vtbase + phf_state_off);
    vt->phf.phf_lookup   = phf_recipe.phf_lookup;
    vt->phf.phf_destruct = &noop_destruct;

    bool alloc_constructed = false;
    if (has_alloc_override)
    {
      if (auto code = type_alloc.allocator_construct(type_alloc_state); code != 0)
      {
        dealloc_from_ctx(ctx, (void*)td, total_size);
        return result_type_fail_create_allocator(code);
      }
      vt->allocator.allocator_destruct = type_alloc.allocator_destruct;
      alloc_constructed                = true;
    }

    // build keys for PHF
    Key* keys = nullptr;
    if (n != 0)
    {
      keys = new (std::nothrow) Key[n];
      if (!keys)
      {
        if (alloc_constructed)
          vt->allocator.allocator_destruct(vt->allocator.state);
        dealloc_from_ctx(ctx, (void*)td, total_size);
        return result_type_fail_mem();
      }

      for (size_t i = 0; i < n; ++i)
      {
        keys[i].key  = members[i].name.data();
        keys[i].size = (uint32_t)members[i].name.size();
      }
    }

    if (auto code = phf_recipe.phf_construct(vt->phf.state, keys, n); code != 0)
    {
      delete[] keys;
      if (alloc_constructed)
        vt->allocator.allocator_destruct(vt->allocator.state);
      dealloc_from_ctx(ctx, (void*)td, total_size);
      return result_type_fail_create_phf(code);
    }
    vt->phf.phf_destruct = phf_recipe.phf_destruct;
    delete[] keys;

    // fill entries + descriptions
    auto* entries = (NamedTypeVTableEntry*)(vtbase + entries_offset);

    size_t desc_cursor = member_desc_start;
    for (size_t i = 0; i < n; ++i)
    {
      const Member& m = members[i];

      desc_cursor = align_up_dyn(desc_cursor, alignof(RTMemberDescription));
      auto* d     = (RTMemberDescription*)(vtbase + desc_cursor);
      desc_cursor += sizeof(RTMemberDescription);

      const auto key_sz  = (uint32_t)m.name.size();
      const auto desc_sz = (uint32_t)m.description.size();

      d->key_size            = key_sz;
      d->hr_description_size = desc_sz;

      if (key_sz != 0)
      {
        std::memcpy(vtbase + desc_cursor, m.name.data(), (size_t)key_sz);
        desc_cursor += (size_t)key_sz;
      }
      vtbase[desc_cursor++] = 0;

      if (desc_sz != 0)
      {
        std::memcpy(vtbase + desc_cursor, m.description.data(), (size_t)desc_sz);
        desc_cursor += (size_t)desc_sz;
      }
      vtbase[desc_cursor++] = 0;

      entries[i].address_or_offset = m.address_or_offset;
      entries[i].type              = m.type;
      entries[i].true_description  = d;
      entries[i].tag               = hash_name(m.name);
    }

    // insert into name table
    try
    {
      ctx->types_table.try_emplace(to_u8string(name), td);
    }
    catch (...)
    {
      vt->phf.phf_destruct(vt->phf.state);
      vt->allocator.allocator_destruct(vt->allocator.state);
      dealloc_from_ctx(ctx, (void*)td, total_size);
      return result_type_fail_mem();
    }
    return result_type_success(td);
  }

  static inline void rt_destroy_any(Type t, void* obj) noexcept;

  static inline void rt_move_any(Type t, void* dst, void* src) noexcept
  {
    if (t->trivial_movable)
    {
      std::memcpy(dst, src, t->type_size);
      return;
    }

    switch_no_default((TypeKind)t->kind)
    {
    case TypeKind::KIND_NAMED:
    {
      STDCOLT_debug_assert(
          t->has_move_fn && t->kind_named.move_fn, "type not movable");
      t->kind_named.move_fn(t, dst, src);
      return;
    }

    case TypeKind::KIND_ARRAY:
    {
      Type elem        = t->kind_array.array_type;
      const uint64_t n = t->kind_array.size;

      const auto d      = (uint8_t*)dst;
      const auto s      = (uint8_t*)src;
      const auto stride = elem->type_size;

      for (uint64_t i = 0; i < n; ++i)
        rt_move_any(elem, d + i * stride, s + i * stride);
      return;
    }
    }
  }

  static inline bool rt_copy_any(Type t, void* dst, const void* src) noexcept
  {
    if (t->trivial_copyable)
    {
      std::memcpy(dst, src, t->type_size);
      return true;
    }

    switch_no_default((TypeKind)t->kind)
    {
    case TypeKind::KIND_NAMED:
    {
      STDCOLT_debug_assert(
          t->has_copy_fn && t->kind_named.copy_fn, "type not copyable");
      return t->kind_named.copy_fn(t, dst, src);
    }
    case TypeKind::KIND_ARRAY:
    {
      Type elem        = t->kind_array.array_type;
      const uint64_t n = t->kind_array.size;

      const auto d      = (uint8_t*)dst;
      const auto s      = (const uint8_t*)src;
      const auto stride = elem->type_size;

      uint64_t i = 0;
      for (; i < n; ++i)
      {
        if (!rt_copy_any(elem, d + i * stride, s + i * stride))
        {
          // rollback already-copied elements
          for (uint64_t j = 0; j < i; ++j)
            rt_destroy_any(elem, d + j * stride);
          return false;
        }
      }
      return true;
    }
    }
  }

  static inline void rt_destroy_any(Type t, void* obj) noexcept
  {
    if (t->trivial_destroy)
      return;

    switch_no_default((TypeKind)t->kind)
    {
    case TypeKind::KIND_NAMED:
      STDCOLT_debug_assert(
          t->kind_named.destroy_fn, "non-trivial destroy without fn");
      t->kind_named.destroy_fn(t, obj);
      return;

    case TypeKind::KIND_ARRAY:
    {
      Type elem         = t->kind_array.array_type;
      const auto p      = (uint8_t*)obj;
      const uint64_t n  = t->kind_array.size;
      const auto stride = elem->type_size;

      for (uint64_t i = 0; i < n; ++i)
        rt_destroy_any(elem, p + i * stride);
      return;
    }
    }
  }

  static void rt_runtime_named_move(
      const TypeDesc* self, void* out, void* src) noexcept
  {
    STDCOLT_debug_assert(
        self && self->kind == (uint64_t)TypeKind::KIND_NAMED, "invalid param");
    const NamedTypeVTable* vt = self->kind_named.vtable;
    STDCOLT_debug_assert(vt != nullptr, "corrupted named type");

    auto entries = vt->entries();
    for (size_t i = 0; i < entries.size(); ++i)
    {
      const auto& e = entries[i];
      uint8_t* d    = (uint8_t*)out + e.address_or_offset;
      uint8_t* s    = (uint8_t*)src + e.address_or_offset;
      rt_move_any(e.type, d, s);
    }
  }
  static bool rt_runtime_named_copy(
      const TypeDesc* self, void* out, const void* src) noexcept
  {
    STDCOLT_debug_assert(
        self && self->kind == (uint64_t)TypeKind::KIND_NAMED, "invalid param");
    const NamedTypeVTable* vt = self->kind_named.vtable;
    STDCOLT_debug_assert(vt != nullptr, "corrupted named type");

    auto entries = vt->entries();
    // copy in order, rollback by destroying already-copied
    for (size_t i = 0; i < entries.size(); ++i)
    {
      const auto& e = entries[i];
      const auto d  = (uint8_t*)out + e.address_or_offset;
      const auto s  = (const uint8_t*)src + e.address_or_offset;

      if (!rt_copy_any(e.type, d, s))
      {
        for (size_t j = 0; j < i; ++j)
        {
          const auto& ej = entries[j];
          uint8_t* dj    = (uint8_t*)out + ej.address_or_offset;
          rt_destroy_any(ej.type, dj);
        }
        return false;
      }
    }
    return true;
  }
  static void rt_runtime_named_destroy(const TypeDesc* self, void* obj) noexcept
  {
    STDCOLT_debug_assert(
        self && self->kind == (uint64_t)TypeKind::KIND_NAMED, "invalid param");
    const NamedTypeVTable* vt = self->kind_named.vtable;
    STDCOLT_debug_assert(vt != nullptr, "corrupted named type");

    auto entries = vt->entries();
    // destroy in reverse order
    for (size_t i = entries.size(); i-- > 0;)
    {
      const auto& e = entries[i];
      // TODO: verify that offset not address
      uint8_t* p = (uint8_t*)obj + e.address_or_offset;
      rt_destroy_any(e.type, p);
    }
  }

  struct LifetimeAggregator
  {
    bool all_triv_move    = true;
    bool all_move         = true;
    bool all_triv_copy    = true;
    bool all_copy         = true;
    bool all_triv_destroy = true;

    void add(Type t) noexcept
    {
      all_triv_move = all_triv_move && t->trivial_movable;
      all_move      = all_move && (t->trivial_movable || t->has_move_fn);

      all_triv_copy = all_triv_copy && t->trivial_copyable;
      all_copy      = all_copy && (t->trivial_copyable || t->has_copy_fn);

      all_triv_destroy = all_triv_destroy && t->trivial_destroy;
    }

    void finalize(NamedLifetime& out) const noexcept
    {
      out.is_trivially_movable  = all_triv_move ? 1 : 0;
      out.is_trivially_copyable = all_triv_copy ? 1 : 0;

      out.move_fn =
          all_triv_move ? nullptr : (all_move ? &rt_runtime_named_move : nullptr);
      out.copy_fn =
          all_triv_copy ? nullptr : (all_copy ? &rt_runtime_named_copy : nullptr);

      out.destroy_fn = all_triv_destroy ? nullptr : &rt_runtime_named_destroy;
    }
  };

  void rt_type_create_runtime_as_declared(
      NamedLifetime& lifetime, uint64_t& align, uint64_t& size, Member* out,
      std::span<const MemberInfo> members) noexcept
  {
    LifetimeAggregator lt;
    for (size_t i = 0; i < members.size(); i++)
    {
      auto& member     = members[i];
      auto& out_member = out[i];

      // to compute trivialness
      lt.add(member.type);
      // populate common data
      out_member.name        = member.name;
      out_member.description = member.description;
      out_member.type        = member.type;

      auto member_align = member.type->type_align;
      auto member_size  = member.type->type_size;

      // align up to the next correct boundary
      size = align_up_dyn(size, member_align);
      // the updated size is the correct offset
      out_member.address_or_offset = size;

      // update the size
      size += member_size;
      // and the alignment to be the max alignment
      align = std::max(align, member_align);
    }
    // final alignment (needed to support arrays)
    size = align_up_dyn(size, align);

    // initialize lifetime
    lt.finalize(lifetime);
  }

  bool rt_type_create_runtime_optimize_size_fast(
      NamedLifetime& lifetime, uint64_t& align, uint64_t& size, Member* out,
      std::span<const MemberInfo> members) noexcept
  {
    try
    {
      LifetimeAggregator lt;
      const size_t n = members.size();

      // remaining indices
      std::vector<size_t> remaining;
      remaining.reserve(n);
      for (size_t i = 0; i < n; ++i)
        remaining.push_back(i);

      auto member_align = [&](size_t i) { return members[i].type->type_align; };
      auto member_size  = [&](size_t i) { return members[i].type->type_size; };

      auto padding_if_placed = [&](uint64_t cur_size, size_t i)
      {
        const auto a       = member_align(i);
        const auto aligned = align_up_dyn(cur_size, a);
        return aligned - cur_size;
      };

      // greedy placement: minimize padding at current offset
      for (size_t out_i = 0; out_i < n; ++out_i)
      {
        size_t best_pos = 0;
        size_t best_idx = remaining[0];

        uint64_t best_pad   = padding_if_placed(size, best_idx);
        uint64_t best_align = member_align(best_idx);
        uint64_t best_size  = member_size(best_idx);

        for (size_t pos = 1; pos < remaining.size(); ++pos)
        {
          const size_t idx = remaining[pos];

          const uint64_t pad = padding_if_placed(size, idx);
          const uint64_t a   = member_align(idx);
          const uint64_t s   = member_size(idx);

          // compare (pad asc), (align desc), (size desc), (index asc)
          bool better = false;
          if (pad < best_pad)
            better = true;
          else if (pad == best_pad)
          {
            if (a > best_align)
              better = true;
            else if (a == best_align)
            {
              if (s > best_size)
                better = true;
              else if (s == best_size)
              {
                if (idx < best_idx)
                  better = true;
              }
            }
          }

          if (better)
          {
            best_pos   = pos;
            best_idx   = idx;
            best_pad   = pad;
            best_align = a;
            best_size  = s;
          }
        }

        const auto& member = members[best_idx];
        auto& out_member   = out[out_i];

        out_member.name        = member.name;
        out_member.description = member.description;
        out_member.type        = member.type;

        // to compute trivialness
        lt.add(member.type);

        const auto a = member.type->type_align;
        const auto s = member.type->type_size;

        size                         = (uint32_t)align_up_dyn(size, a);
        out_member.address_or_offset = size;

        size += s;
        align = std::max(align, a);

        remaining[best_pos] = remaining.back();
        remaining.pop_back();
      }
      size = align_up_dyn(size, align);

      // initialize lifetime
      lt.finalize(lifetime);

      return true;
    }
    catch (...)
    {
      // allocation failure
      return false;
    }
  }

  TypeResult rt_type_create_runtime(
      RuntimeContext* ctx, std::span<const char8_t> name,
      std::span<const MemberInfo> members, RuntimeTypeLayout layout,
      const RecipeAllocator* alloc_override,
      const RecipePerfectHashFunction* phf_override) noexcept
  {
    if (layout >= RuntimeTypeLayout::_RuntimeTypeLayout_end)
      return result_type_invalid_param();

    uint64_t align   = 1;
    uint64_t size    = 0;
    auto members_ptr = new (std::nothrow) Member[members.size()];
    // verify allocation
    if (members_ptr == nullptr)
      return result_type_fail_mem();

    NamedLifetime lt{};
    if (layout == RuntimeTypeLayout::LAYOUT_AS_DECLARED)
    {
      // cannot fail
      rt_type_create_runtime_as_declared(lt, align, size, members_ptr, members);
    }
    else if (layout == RuntimeTypeLayout::LAYOUT_OPTIMIZE_SIZE_FAST)
    {
      // may fail due to allocation failure
      if (!rt_type_create_runtime_optimize_size_fast(
              lt, align, size, members_ptr, members))
      {
        delete[] members_ptr;
        return result_type_fail_mem();
      }
    }

    auto type = rt_type_create(
        ctx, name, {members_ptr, members.size()}, align, size, &lt, alloc_override,
        phf_override);
    delete[] members_ptr;
    return type;
  }

  static inline bool name_equals(
      std::span<const char8_t> a, const char8_t* b_cstr, uint32_t b_size) noexcept
  {
    if (a.size() != (size_t)b_size)
      return false;
    if (b_size == 0)
      return true;
    return std::memcmp(a.data(), b_cstr, (size_t)b_size) == 0;
  }

  LookupResult rt_type_lookup_fast(
      Type type_to_lookup, std::span<const char8_t> name,
      Type expected_type) noexcept
  {
    if (!type_to_lookup || type_to_lookup->kind != (uint64_t)TypeKind::KIND_NAMED)
      return lookup_expected_named();

    const NamedTypeVTable* vt = type_to_lookup->kind_named.vtable;
    if (!vt || vt->count_members == 0)
      return lookup_not_found();

    const Key k{(const void*)name.data(), (uint32_t)name.size()};
    const uint64_t idx = vt->phf.phf_lookup(vt->phf.state, k);
    STDCOLT_debug_assert(idx < vt->count_members, "invalid return from PHF");

    const auto entries            = vt->entries();
    const NamedTypeVTableEntry& e = entries[idx];

    // type safety is guaranteed by Type pointer equality
    if (e.type != expected_type)
      return lookup_mismatch(e.type);

    if (const uint64_t t = hash_name(name); e.tag != t)
      return lookup_not_found();

    return lookup_found(e.address_or_offset);
  }

  LookupResult rt_type_lookup(
      Type type_to_lookup, std::span<const char8_t> name,
      Type expected_type) noexcept
  {
    if (!type_to_lookup || type_to_lookup->kind != (uint64_t)TypeKind::KIND_NAMED)
      return lookup_expected_named();

    const NamedTypeVTable* vt = type_to_lookup->kind_named.vtable;
    STDCOLT_debug_assert(vt != nullptr, "corrupted type");
    if (vt->count_members == 0)
      return lookup_not_found();

    const Key k{(const void*)name.data(), (uint32_t)name.size()};
    const uint64_t idx = vt->phf.phf_lookup(vt->phf.state, k);
    STDCOLT_debug_assert(idx < vt->count_members, "invalid return from PHF");

    const auto entries            = vt->entries();
    const NamedTypeVTableEntry& e = entries[idx];

    if (e.type != expected_type)
      return lookup_mismatch(e.type);

    const RTMemberDescription* d = e.true_description;
    if (!d)
      return lookup_not_found();

    if (!name_equals(name, d->key_c_str(), d->key_size))
      return lookup_not_found();

    return lookup_found(e.address_or_offset);
  }

  bool rt_register_set_type(RuntimeContext* ctx, OpaqueTypeID id, Type type) noexcept
  {
    if (!ctx || !id || !type || type->owner != ctx)
      return false;
    try
    {
      auto [it, inserted] = ctx->registered_type_table.emplace(id, type);
      if (!inserted)
        it->second = type;
      return true;
    }
    catch (...)
    {
      return false;
    }
  }

  Type rt_register_get_type(RuntimeContext* ctx, OpaqueTypeID id) noexcept
  {
    if (!ctx || !id)
      return nullptr;
    auto it = ctx->registered_type_table.find(id);
    if (it == ctx->registered_type_table.end())
      return nullptr;
    return it->second;
  }

  static inline PreparedMember pm_invalid() noexcept
  {
    return PreparedMember{nullptr, nullptr, 0, 0};
  }

  PreparedMember rt_prepare_member(
      Type owner_named, std::span<const char8_t> member_name,
      Type expected_type) noexcept
  {
    PreparedMember pm{};

    if (!owner_named || owner_named->kind != (uint64_t)TypeKind::KIND_NAMED)
      return pm_invalid();

    const NamedTypeVTable* vt = owner_named->kind_named.vtable;
    if (!vt || vt->count_members == 0 || !vt->phf.phf_lookup)
      return pm_invalid();

    const Key k{(const void*)member_name.data(), (uint32_t)member_name.size()};
    const uint64_t id = vt->phf.phf_lookup(vt->phf.state, k);

    STDCOLT_debug_assert(id < vt->count_members, "invalid return from PHF");

    pm.owner    = owner_named;
    pm.expected = expected_type;
    pm.tag1     = id;
    pm.tag2     = hash_name(member_name);
    return pm;
  }

  LookupResult rt_resolve_prepared_member(const PreparedMember& pm) noexcept
  {
    if (!pm.owner || pm.owner->kind != (uint64_t)TypeKind::KIND_NAMED)
      return lookup_expected_named();

    const NamedTypeVTable* vt = pm.owner->kind_named.vtable;
    if (pm.tag1 >= vt->count_members)
      return lookup_not_found();

    const auto entries            = vt->entries();
    const NamedTypeVTableEntry& e = entries[pm.tag1];

    if (e.type != pm.expected)
      return lookup_mismatch(e.type);
    if (e.tag != pm.tag2)
      return lookup_not_found();
    return lookup_found(e.address_or_offset);
  }

  static inline Allocator ctx_allocator(RuntimeContext* ctx) noexcept
  {
    Allocator a{};
    a.state              = ctx->default_alloc_state.ptr();
    a.allocator_alloc    = ctx->default_alloc_recipe.allocator_alloc;
    a.allocator_dealloc  = ctx->default_alloc_recipe.allocator_dealloc;
    a.allocator_destruct = &noop_destruct;
    return a;
  }

  static inline Allocator instance_allocator_for(Type type) noexcept
  {
    RuntimeContext* ctx = type->owner;
    STDCOLT_debug_assert(ctx != nullptr, "invalid type");

    switch ((TypeKind)type->kind)
    {
    case TypeKind::KIND_NAMED:
    {
      const NamedTypeVTable* vt = type->kind_named.vtable;
      STDCOLT_debug_assert(vt != nullptr, "named type without vtable");
      return vt->allocator; // may be override or ctx default
    }

    case TypeKind::KIND_ARRAY:
      STDCOLT_debug_assert(
          type->kind_array.array_type != nullptr, "array missing element type");
      // recurse so arrays of arrays end up using the base element's named allocator
      return instance_allocator_for(type->kind_array.array_type);

    default:
      return ctx_allocator(ctx);
    }
  }

  static inline bool is_type_movable(Type type) noexcept
  {
    return type->trivial_movable || type->has_move_fn;
  }
  static inline bool is_type_copyable(Type type) noexcept
  {
    return type->trivial_copyable || type->has_copy_fn;
  }
  static inline void* to_address_in_sbo(Type type, void* sbo) noexcept
  {
    // fast case, object does not need padding
    if (type->type_align <= VALUE_SBO_ALIGN && type->type_size <= VALUE_SBO_SIZE)
      return sbo;

    // must fit EVEN WITH THE WORST CASE PADDING...
    // if not true: moving from a Value to another that are not aligned
    // in the same way may convert an inline value to a heap stored value
    // which is unacceptable due to moving always being guaranteed to succeed.
    if (type->type_size + (type->type_align - 1) > VALUE_SBO_SIZE)
      return nullptr;

    // now safe to compute aligned placement: it will always fit
    const auto base         = reinterpret_cast<uintptr_t>(sbo);
    const uintptr_t aligned = align_up_dyn(base, type->type_align);
    return reinterpret_cast<void*>(aligned);
  }
  static inline bool is_in_sbo(const Value* val_addr) noexcept
  {
    const auto addr = (uintptr_t)val_addr->header.address;
    const auto buf  = (uintptr_t)val_addr->inline_buffer;
    // exact check in buffer...
    return buf <= addr && addr < buf + VALUE_SBO_SIZE;
  }
  static inline bool is_in_heap(const Value* val_addr) noexcept
  {
    return val_addr->header.address != nullptr && !is_in_sbo(val_addr);
  }

  bool val_construct(Value* out, Type type) noexcept
  {
    if (type == nullptr)
    {
      val_construct_empty(out);
      return true;
    }

    void* object = nullptr;
    // if the type is movable and an object may fit under the worst-case
    // padding rule at runtime in the SBO, store it inline
    if (is_type_movable(type)
        && (object = to_address_in_sbo(type, out->inline_buffer)))
    {
      // sbo, nothing needed
    }
    else // heap allocation
    {
      Allocator a = instance_allocator_for(type);
      Block blk   = a.allocator_alloc(a.state, type->type_size, type->type_align);
      if (!blk.ptr)
        return val_construct_empty(out), false;
      object = blk.ptr;
    }
    out->header.type    = type;
    out->header.address = object;
    return true;
  }

  void val_construct_empty(Value* out) noexcept
  {
    out->header.address = nullptr;
    out->header.type    = nullptr;
  }

  void val_construct_from_move(Value* out, Value* to_move) noexcept
  {
    STDCOLT_pre(out != nullptr, "expected non-null parameter");
    STDCOLT_pre(to_move != nullptr, "expected non-null parameter");
    STDCOLT_pre(out != to_move, "invalid parameters");

    auto type = to_move->header.type;
    // if empty, make result empty
    if (type == nullptr)
      return val_construct_empty(out);
    // if in heap, we can simply copy the pointer and mark to_move empty.
    if (is_in_heap(to_move))
    {
      // copy header
      out->header = to_move->header;
      return val_construct_empty(to_move);
    }
    // object in SBO
    STDCOLT_debug_assert(is_type_movable(type), "expected movable type");
    // we need to recompute the address into the SBO as `out` and `to_move`
    // may have different alignment. to_address_in_sbo is guaranteed to
    // not fail in this case
    auto new_addr = to_address_in_sbo(type, out->inline_buffer);
    STDCOLT_debug_assert(new_addr != nullptr, "corrupted value received");

    if (type->trivial_movable)
      memcpy(new_addr, to_move->header.address, type->type_size);
    else // type has a move function
    {
      STDCOLT_debug_assert(
          type->kind == (uint8_t)TypeKind::KIND_NAMED, "expected named types");
      type->kind_named.move_fn(type, new_addr, to_move->header.address);
    }

    out->header.type    = type;
    out->header.address = new_addr;
    val_construct_empty(to_move);
  }

  bool val_construct_from_copy(Value* out, const Value* to_copy) noexcept
  {
    STDCOLT_pre(out != nullptr, "expected non-null parameter");
    STDCOLT_pre(to_copy != nullptr, "expected non-null parameter");

    // do not copy if same value
    if (to_copy == out)
      return true;

    auto type = to_copy->header.type;
    // if empty, make result empty
    if (type == nullptr)
      return val_construct_empty(out), true;
    // if the type is not copyable, return early...
    if (!is_type_copyable(type))
      return val_construct_empty(out), false;

    void* new_addr     = nullptr;
    bool heap_allocate = false;
    if (is_in_heap(to_copy))
    {
      heap_allocate = true;

      Allocator a = instance_allocator_for(type);
      Block blk   = a.allocator_alloc(a.state, type->type_size, type->type_align);
      if (!blk.ptr)
        return val_construct_empty(out), false;

      new_addr = blk.ptr;
    }
    else // object in SBO
    {
      // we need to recompute the address into the SBO as `out` and `to_copy`
      // may have different alignment. to_address_in_sbo is guaranteed to
      // not fail in this case
      new_addr = to_address_in_sbo(type, out->inline_buffer);
      STDCOLT_debug_assert(new_addr != nullptr, "corrupted value received");
    }

    if (type->trivial_copyable)
      memcpy(new_addr, to_copy->header.address, type->type_size);
    else
    {
      STDCOLT_debug_assert(
          type->kind == (uint8_t)TypeKind::KIND_NAMED, "expected named types");
      // if copy is not successful, clear `out` and return false
      if (!type->kind_named.copy_fn(type, new_addr, to_copy->header.address))
      {
        if (heap_allocate)
        {
          Allocator a = instance_allocator_for(type);
          a.allocator_dealloc(a.state, Block{new_addr, type->type_size});
        }
        val_construct_empty(out);
        return false;
      }
    }

    out->header.type    = type;
    out->header.address = new_addr;
    return true;
  }

  void val_destroy(Value* val) noexcept
  {
    // noop for empty values
    if (val == nullptr || val->header.type == nullptr)
      return;
    auto type = val->header.type;
    if (!type->trivial_destroy)
    {
      STDCOLT_debug_assert(
          type->kind == (uint8_t)TypeKind::KIND_NAMED, "expected named types");
      // call destructor
      type->kind_named.destroy_fn(type, val->header.address);
    }
    if (is_in_heap(val))
    {
      Allocator a = instance_allocator_for(type);
      a.allocator_dealloc(a.state, Block{val->header.address, type->type_size});
    }
    val_construct_empty(val);
  }
} // namespace stdcolt::ext::rt
