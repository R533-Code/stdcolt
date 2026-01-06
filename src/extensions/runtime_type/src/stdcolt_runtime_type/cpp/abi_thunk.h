#ifndef __HG_STDCOLT_EXT_RUNTIME_TYPE_CPP_ABI_THUNK
#define __HG_STDCOLT_EXT_RUNTIME_TYPE_CPP_ABI_THUNK

#include <type_traits>
#include <concepts>

namespace stdcolt::ext::rt
{
  /***********************/
  // ABI STABILITY
  /***********************/

  template<typename T>
  struct abi_param
  {
    using type = T;
  };

  template<typename T>
  struct abi_param<T&>
  {
    using type = T*;
  };

  template<typename T>
  struct abi_param<const T&>
  {
    using type = const T*;
  };

  template<typename T>
  struct abi_param<T&&>
  {
    using type = T*;
  };

  template<typename T>
  using abi_param_t = typename abi_param<T>::type;

  template<typename T>
  decltype(auto) from_abi(abi_param_t<T> v) noexcept
  {
    if constexpr (std::is_lvalue_reference_v<T>)
      return *v;
    else if constexpr (std::is_rvalue_reference_v<T>)
      return std::move(*v);
    else
      return v;
  }

  template<typename R>
  struct abi_ret
  {
    using type = R;
  };

  template<typename R>
  struct abi_ret<R&>
  {
    using type = R*;
  };

  template<typename R>
  struct abi_ret<const R&>
  {
    using type = const R*;
  };

  template<typename R>
  using abi_ret_t = typename abi_ret<R>::type;

  template<typename R>
  abi_ret_t<R> to_abi_ret(R&& r) noexcept
  {
    if constexpr (std::is_lvalue_reference_v<R> || std::is_rvalue_reference_v<R>)
      return std::addressof(r);
    else
      return static_cast<R&&>(r);
  }
} // namespace stdcolt::ext::rt

#endif // !__HG_STDCOLT_EXT_RUNTIME_TYPE_CPP_ABI_THUNK
