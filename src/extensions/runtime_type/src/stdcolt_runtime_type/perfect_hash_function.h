/*****************************************************************/ /**
 * @file   perfect_hash_function.h
 * @brief  Contains perfect hash function recipes (for type erasure).
 * @author Raphael Dib Nehme
 * @date   December 2025
 *********************************************************************/
#ifndef __HG_STDCOLT_EXT_RUNTIME_TYPE_PERFECT_HASH_FUNCTION
#define __HG_STDCOLT_EXT_RUNTIME_TYPE_PERFECT_HASH_FUNCTION

#include <cstdint>
#include <stdcolt_runtime_type_export.h>

namespace stdcolt::ext::rt
{
  /// @brief A key passed to perfect hash functions
  struct Key
  {
    /// @brief The key
    const void* key;
    /// @brief The size of the key
    uint32_t size;
  };

  /// @brief Type erased perfect hash function builder.
  struct RecipePerfectHashFunction
  {
    /// @brief Size of the resulting perfect hash function (may be zero)
    uint32_t phf_sizeof;
    /// @brief The alignment of the perfect hash function (>= 1)
    uint32_t phf_alignof;
    /// @brief Constructs a perfect hash function object.
    /// First argument, the storage of size `phf_sizeof`.
    /// Second argument, the array of keys.
    /// Third argument, the size of the array of keys.
    /// Returns 0 on success.
    int32_t (*phf_construct)(void*, const Key*, uint64_t) noexcept;
    /// @brief Destructs a perfect hash function object.
    /// First argument, the storage passed to `phf_construct`.
    /// @pre May only be called if `phf_construct` returned 0.
    void (*phf_destruct)(void*) noexcept;
    /// @brief Applies the perfect hash function on a key.
    /// First argument, the storage passed to `phf_construct`.
    /// Second argument, the key.
    /// The return value of this function is guaranteed to be in the range
    /// [0, size of array passed to `phf_construct`), even for keys that
    /// do not exist in the initial set.
    /// @pre May only be called if `phf_construct` returned 0.
    uint64_t (*phf_lookup)(void*, Key) noexcept;
  };

  /// @brief Per-VTable PHF data
  struct PerfectHashFunction
  {
    /// @brief Pointer to the type erased perfect hash function
    void* state;
    /// @brief Perfect hash function lookup
    uint64_t (*phf_lookup)(void*, Key) noexcept;
    /// @brief Pointer to the type erased destruct hash function
    void (*phf_destruct)(void*) noexcept;
  };

  /// @brief Returns the default perfect hash function recipe.
  /// @return The perfect hash function recipe
  STDCOLT_RUNTIME_TYPE_EXPORT
  RecipePerfectHashFunction default_perfect_hash_function() noexcept;
} // namespace stdcolt::ext::rt

#endif // !__HG_STDCOLT_EXT_RUNTIME_TYPE_PERFECT_HASH_FUNCTION
