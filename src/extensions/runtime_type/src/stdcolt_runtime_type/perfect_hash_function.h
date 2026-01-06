/*****************************************************************/ /**
 * @file   perfect_hash_function.h
 * @brief  Contains perfect hash function recipes (for type erasure).
 * This header is suitable to be consumed by C compilers.
 * @author Raphael Dib Nehme
 * @date   December 2025
 *********************************************************************/
#ifndef __HG_STDCOLT_EXT_RUNTIME_TYPE_PERFECT_HASH_FUNCTION
#define __HG_STDCOLT_EXT_RUNTIME_TYPE_PERFECT_HASH_FUNCTION

#include <stdint.h>
#include <stdcolt_runtime_type_export.h>

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

  /// @brief A key passed to perfect hash functions
  typedef struct
  {
    /// @brief The key
    const void* key;
    /// @brief The size of the key
    uint64_t size;
  } stdcolt_ext_rt_Key;

  /// @brief Type erased perfect hash function builder.
  typedef struct
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
    int32_t (*phf_construct)(void*, const stdcolt_ext_rt_Key*, uint64_t);
    /// @brief Destructs a perfect hash function object.
    /// First argument, the storage passed to `phf_construct`.
    /// @pre May only be called if `phf_construct` returned 0.
    void (*phf_destruct)(void*);
    /// @brief Applies the perfect hash function on a key.
    /// First argument, the storage passed to `phf_construct`.
    /// Second argument, the key.
    /// The return value of this function is guaranteed to be in the range
    /// [0, size of array passed to `phf_construct`), even for keys that
    /// do not exist in the initial set.
    /// @pre May only be called if `phf_construct` returned 0.
    uint64_t (*phf_lookup)(void*, const stdcolt_ext_rt_Key*);
  } stdcolt_ext_rt_RecipePerfectHashFunction;

  /// @brief Per-VTable PHF data
  typedef struct
  {
    /// @brief Pointer to the type erased perfect hash function
    void* state;
    /// @brief Perfect hash function lookup
    uint64_t (*phf_lookup)(void*, const stdcolt_ext_rt_Key*);
    /// @brief Pointer to the type erased destruct hash function
    void (*phf_destruct)(void*);
  } stdcolt_ext_rt_PerfectHashFunction;

  /// @brief Returns the default perfect hash function recipe.
  /// @return The perfect hash function recipe
  STDCOLT_RUNTIME_TYPE_EXPORT
  stdcolt_ext_rt_RecipePerfectHashFunction stdcolt_ext_rt_default_perfect_hash_function();

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // !__HG_STDCOLT_EXT_RUNTIME_TYPE_PERFECT_HASH_FUNCTION
