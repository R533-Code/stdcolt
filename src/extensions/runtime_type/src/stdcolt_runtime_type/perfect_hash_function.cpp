/*****************************************************************/ /**
 * @file   perfect_hash_function.cpp
 * @brief  Contains the implementation of `perfect_hash_function.h`.
 * @author Raphael Dib Nehme
 * @date   December 2025
 *********************************************************************/
#include "./perfect_hash_function.h"
#include <string_view>
#include <string>
#include <unordered_map>

namespace stdcolt::ext::rt
{
  static std::string_view to_sv(Key key) noexcept
  {
    return {(const char*)key.key, key.size};
  }

  struct transparent_hash
  {
    using is_transparent = void;
    using hash_type      = std::hash<std::string_view>;

    size_t operator()(std::string_view sv) const noexcept { return hash_type{}(sv); }
    size_t operator()(Key sv) const noexcept { return hash_type{}(to_sv(sv)); }
    size_t operator()(const char* s) const noexcept
    {
      return hash_type{}(std::string_view{s});
    }
    size_t operator()(const std::string& s) const noexcept
    {
      return hash_type{}(std::string_view{s});
    }
  };

  using default_phf =
      std::unordered_map<std::string, uint64_t, transparent_hash, std::equal_to<>>;

  static int32_t default_phf_construct(
      void* storage, const Key* array_key, uint64_t array_size) noexcept
  {
    default_phf* ptr = nullptr;
    try
    {
      ptr = new (storage) default_phf();
      for (size_t i = 0; i < array_size; i++)
        ptr->emplace(to_sv(array_key[i]), i);
      return 0;
    }
    catch (...)
    {
      if (ptr)
        ptr->~default_phf();
      return -1;
    }
  }

  static void default_phf_destruct(void* storage) noexcept
  {
    try
    {
      ((default_phf*)storage)->~default_phf();
    }
    catch (...)
    {
      // do nothing
    }
  }

  static uint64_t default_phf_lookup(void* storage, Key key) noexcept
  {
    auto ptr = ((default_phf*)storage);
    auto ret = ptr->find(key);
    if (ret == ptr->end())
      return 0;
    return ret->second;
  }

  RecipePerfectHashFunction default_perfect_hash_function() noexcept
  {
    return {
        .phf_sizeof    = sizeof(default_phf),
        .phf_construct = &default_phf_construct,
        .phf_destruct  = &default_phf_destruct,
        .phf_lookup    = &default_phf_lookup,
    };
  }
} // namespace stdcolt::ext::rt
