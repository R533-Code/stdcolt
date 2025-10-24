#ifndef __HG_STDCOLT_TYPE_ERASE_TYPE_ERASE
#define __HG_STDCOLT_TYPE_ERASE_TYPE_ERASE

#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <compare>
#include <stdcolt_contracts/contracts.h>
#include <stdcolt_macros/for_each.h>
#include <stdcolt_macros/tuple.h>

/// @brief (int, x) -> int x
#define __COLT_MAKE_PARAM(FN_2D) \
  STDCOLT_2D_1(FN_2D)            \
  STDCOLT_2D_2(FN_2D)
/// @brief (int, x) -> x
#define __COLT_MAKE_ARG(FN_2D) STDCOLT_2D_2(FN_2D)
/// @brief (int, x) -> int
#define __COLT_MAKE_TYPE(FN_2D) STDCOLT_2D_1(FN_2D)
/// @brief ANY -> + 1
#define __COLT_MAKE_PLUS_1(ANY) +1ull

/// @brief ((int, x), (int, y)) -> int x, int y
#define __COLT_MAKE_PARAMS(...) \
  STDCOLT_FOR_EACH_COMMA2(__COLT_MAKE_PARAM, __VA_ARGS__)
/// @brief ((int, x), (int, y)) -> , int x, int y
#define __COLT_MAKE_PARAMS_WITH_COMMA(...) \
  __VA_OPT__(, ) STDCOLT_FOR_EACH_COMMA2(__COLT_MAKE_PARAM, __VA_ARGS__)
/// @brief ((int, x), (int, y)) -> x, y
#define __COLT_MAKE_ARGS(...) STDCOLT_FOR_EACH_COMMA2(__COLT_MAKE_ARG, __VA_ARGS__)
/// @brief ((int, x), (int, y)) -> , x, y
#define __COLT_MAKE_ARGS_WITH_COMMA(...) \
  __VA_OPT__(, ) STDCOLT_FOR_EACH_COMMA2(__COLT_MAKE_ARG, __VA_ARGS__)
/// @brief ((int, x), (int, y)) -> int, int
#define __COLT_MAKE_TYPES(...) STDCOLT_FOR_EACH_COMMA2(__COLT_MAKE_TYPE, __VA_ARGS__)
/// @brief ((int, x), (int, y)) -> , int, int
#define __COLT_MAKE_TYPES_WITH_COMMA(...) \
  __VA_OPT__(, ) STDCOLT_FOR_EACH_COMMA2(__COLT_MAKE_TYPE, __VA_ARGS__)

#define __COLT_MAKE_COUNT_PACK(...) \
  (0ull STDCOLT_FOR_EACH2(__COLT_MAKE_PLUS_1, __VA_ARGS__))

#define __COLT_MAKE_REQUIRE_CLAUSE(FN_4D)                                      \
  requires(STDCOLT_2D_1(STDCOLT_4D_4(FN_4D))                                   \
               __ABI_T __STDCOLT_TYPE_ERASE_arg __COLT_MAKE_PARAMS_WITH_COMMA( \
                   STDCOLT_DEPAREN(STDCOLT_4D_3(FN_4D)))) {                    \
    {                                                                          \
      __STDCOLT_TYPE_ERASE_arg.STDCOLT_4D_2(FN_4D)(                            \
          __COLT_MAKE_ARGS(STDCOLT_DEPAREN(STDCOLT_4D_3(FN_4D))))              \
    } -> std::same_as<STDCOLT_4D_1(FN_4D)>;                                    \
  }

#define __COLT_MAKE_TYPE_ERASED_FN_PTRS(FN_4D)                              \
  STDCOLT_4D_1(FN_4D) (                                                     \
      *STDCOLT_CC(STDCOLT_4D_2(FN_4D), STDCOLT_2D_2(STDCOLT_4D_4(FN_4D))))( \
      STDCOLT_2D_1(STDCOLT_4D_4(FN_4D)) void* __COLT_MAKE_TYPES_WITH_COMMA( \
          STDCOLT_DEPAREN(STDCOLT_4D_3(FN_4D))));

#define __COLT_MAKE_TYPE_ERASED_LAMBDAS(FN_4D)                                \
  +[](__COLT_MAKE_PARAMS(                                                     \
       (STDCOLT_2D_1(STDCOLT_4D_4(FN_4D)) void*, __STDCOLT_TYPE_ERASE_arg),   \
       STDCOLT_DEPAREN(STDCOLT_4D_3(FN_4D)))) -> decltype(auto)               \
  {                                                                           \
    return (reinterpret_cast<STDCOLT_2D_1(STDCOLT_4D_4(FN_4D)) __ABI_T*>(     \
                __STDCOLT_TYPE_ERASE_arg)                                     \
                ->STDCOLT_4D_2(FN_4D)(                                        \
                    __COLT_MAKE_ARGS(STDCOLT_DEPAREN(STDCOLT_4D_3(FN_4D))))); \
  }

#define __COLT_MAKE_METHOD(REQUIRE_CLAUSE, FN_4D)                               \
  STDCOLT_4D_1(FN_4D)                                                           \
  STDCOLT_4D_2(FN_4D)(__COLT_MAKE_PARAMS(STDCOLT_DEPAREN(STDCOLT_4D_3(FN_4D)))) \
      STDCOLT_2D_1(STDCOLT_4D_4(FN_4D)) REQUIRE_CLAUSE(FN_4D)                   \
  {                                                                             \
    return get_vtable()->STDCOLT_CC(                                            \
        STDCOLT_4D_2(FN_4D), STDCOLT_2D_2(STDCOLT_4D_4(FN_4D)))(                \
        get_ptr()                                                               \
            __COLT_MAKE_ARGS_WITH_COMMA(STDCOLT_DEPAREN(STDCOLT_4D_3(FN_4D)))); \
  }

#define __COLT_MAKE_METHOD_EMPTY_REQUIRE(FN_4D)
#define __COLT_MAKE_METHOD_CONST_REQUIRE(FN_4D) \
  requires(!IS_CONST || std::is_const_v<STDCOLT_2D_1(STDCOLT_4D_4(FN_4D)) int>)

#define __STDCOLT_TYPE_ERASE_TRY_RETHROW(TRY, BEFORE_RETHROW) \
  do                                                          \
  {                                                           \
    try                                                       \
    {                                                         \
      TRY;                                                    \
    }                                                         \
    catch (...)                                               \
    {                                                         \
      BEFORE_RETHROW;                                         \
      throw;                                                  \
    }                                                         \
  } while (0)

#define __COLT_MAKE_ABI_TYPE_CONSTRUCTOR(NAME, PARAM_SPEC, REQUIRES, VTABLE_CALL) \
  NAME(NAME PARAM_SPEC other)                                                     \
  requires(REQUIRES)                                                              \
      : _vtable(other._vtable)                                                    \
  {                                                                               \
    const auto sizeof_type = other.get_vtable()->TypeSize::value;                 \
    if (sizeof_type <= INLINE_BUFFER_SIZE)                                        \
      VTABLE_CALL(_buffer, other._buffer);                                        \
    else                                                                          \
    {                                                                             \
      auto ptr = CUSTOM.alloc_fn(sizeof_type);                                    \
      if (ptr == nullptr)                                                         \
        throw std::bad_alloc{};                                                   \
      __STDCOLT_TYPE_ERASE_TRY_RETHROW(                                           \
          VTABLE_CALL(ptr, other._heap), CUSTOM.dealloc_fn(ptr));                 \
      _heap = ptr;                                                                \
    } /* no need to mark vtable* as we already copied it with it */               \
  }

/// @brief Contains utilities and ABI-stable types defined through `STDCOLT_TYPE_ERASE_DECLARE_TYPE`
namespace stdcolt::type_erase
{
  /// @brief Allocation function that always fails
  /// @note To be paired with `fail_dealloc`!
  /// @param size The size of the allocation
  /// @return Always returns nullptr
  inline void* fail_alloc([[maybe_unused]] size_t size)
  {
    return nullptr;
  }
  /// @brief Deallocation function that does nothing.
  /// @note To be paired with `fail_alloc`!
  /// @param to_free The allocation to free (always nullptr!)
  inline void fail_dealloc([[maybe_unused]] void* to_free)
  {
    STDCOLT_debug_assert(to_free == nullptr, "expected nullptr");
  }

  /// @brief Customization for the generated vtable
  struct CustomizeVTable
  {
    /// @brief True if the wrapper should be copy constructible
    bool is_copy_constructible;
    /// @brief True if the wrapper should be movable
    bool is_move_constructible;
  };

  /// @brief Customization for STDCOLT_TYPE_ERASE_DECLARE_TYPE
  struct CustomizeABI
  {
    /// @brief The size of the small stack buffer in bytes.
    /// @note This will be rounded up to `alignof(void*)` automatically
    size_t inline_buffer_size;
    /// @brief True if the wrapper should be copy constructible
    bool is_copy_constructible = false;
    /// @brief True if the wrapper should be movable
    bool is_move_constructible = true;
    /// @brief The allocation function
    void* (*alloc_fn)(size_t) = +[](size_t size) { return std::malloc(size); };
    /// @brief The deallocation function
    void (*dealloc_fn)(void*) = +[](void* ptr) { std::free(ptr); };

    /// @brief Converts the current CustomizeABI to a CustomizeVTable.
    /// This is to remove members that are not useful for vtable generation.
    /// @return CustomizeVTable
    constexpr CustomizeVTable to_customize_vtable() const noexcept
    {
      return {is_copy_constructible, is_move_constructible};
    }
  };

  /// @brief Type erased destructor call
  using type_erased_destructor_t = void (*)(void*);
  /// @brief Type erased copy constructor call (out, to_copy)
  using type_erased_copy_constructor_t = void (*)(void*, const void*);
  /// @brief Type erased move constructor call (out, to_copy)
  using type_erased_move_constructor_t = void (*)(void*, void*);

  /// @brief Type used for EBO in v-tables
  /// @note See `https://en.cppreference.com/w/cpp/language/ebo.html`
  /// @tparam __ABI_T The type to contain from if INHERIT is true
  /// @tparam INHERIT True if the type
  template<bool INHERIT, typename __ABI_T>
  struct TypeOrEmpty;

  /// @brief Type used for EBO in v-tables
  /// @tparam __ABI_T The type to contain
  template<typename __ABI_T>
  struct TypeOrEmpty<true, __ABI_T>
  {
    __ABI_T value;
  };

  /// @brief Type used for EBO in v-tables
  /// @tparam __ABI_T The type to contain
  template<typename __ABI_T>
  struct TypeOrEmpty<false, __ABI_T>
  {
  };

  /// @brief Contains the number of functions in a v-table
  /// @note This type is needed to 'inherit' a size_t
  struct FunctionCount
  {
    /// @brief The number of functions in a v-table
    size_t value;
  };
  /// @brief Contains the sizeof the type represented in the v-table
  /// @note This type is needed to 'inherit' a size_t
  struct TypeSize
  {
    /// @brief sizeof the type represented in the v-table
    size_t value;
  };
  /// @brief Contains the type erased destructor
  struct DestructorFn
  {
    /// @brief Function pointer to type erased destructor
    type_erased_destructor_t value;

    /// @brief Assigns this the type erased destructor of __ABI_T
    /// @tparam __ABI_T The type whose destructor to type erase
    template<typename __ABI_T>
    constexpr void make_erased_for() noexcept
    {
      value = +[](void* arg) { reinterpret_cast<__ABI_T*>(arg)->~__ABI_T(); };
    }
  };
  /// @brief Contains the type erased copy constructor
  struct CopyConstructorFn
  {
    /// @brief Function pointer to type erased copy constructor
    type_erased_copy_constructor_t value;

    /// @brief Assigns this the type erased copy constructor of __ABI_T
    /// @tparam __ABI_T The type whose copy constructor to type erase
    template<typename __ABI_T>
    constexpr void make_erased_for() noexcept
    {
      // placement new into out
      value = +[](void* out, const void* in)
      { new (out) __ABI_T(*reinterpret_cast<const __ABI_T*>(in)); };
    }
  };
  /// @brief Contains the type erased move constructor
  struct MoveConstructorFn
  {
    /// @brief Function pointer to type erased move constructor
    type_erased_move_constructor_t value;

    /// @brief Assigns this the type erased move constructor of __ABI_T
    /// @tparam __ABI_T The type whose move constructor to type erase
    template<typename __ABI_T>
    constexpr void make_erased_for() noexcept
    {
      // placement new into out
      value = +[](void* out, void* in)
      {
        new (out) __ABI_T(static_cast<__ABI_T&&>(*reinterpret_cast<__ABI_T*>(in)));
      };
    }
  };
} // namespace stdcolt::type_erase

#define STDCOLT_TYPE_ERASE_DECLARE_TYPE(                                             \
    NAMESPACE, CONCEPT_NAME, TYPENAME, CUSTOMIZATION, FN1, ...)                      \
  namespace NAMESPACE                                                                \
  {                                                                                  \
    static constexpr auto STDCOLT_CC(TYPENAME, Customization) = CUSTOMIZATION;       \
    static constexpr auto STDCOLT_CC(TYPENAME, VTableCustomization) =                \
        STDCOLT_CC(TYPENAME, Customization).to_customize_vtable();                   \
    template<                                                                        \
        typename __ABI_T, stdcolt::type_erase::CustomizeVTable CUSTOM =              \
                              NAMESPACE::STDCOLT_CC(TYPENAME, VTableCustomization)>  \
    concept CONCEPT_NAME = STDCOLT_FOR_EACH_SYMBOL(                                  \
        __COLT_MAKE_REQUIRE_CLAUSE, &&, FN1 __VA_OPT__(, ) __VA_ARGS__);             \
    template<                                                                        \
        stdcolt::type_erase::CustomizeVTable CUSTOM =                                \
            STDCOLT_CC(TYPENAME, VTableCustomization)>                               \
    struct STDCOLT_CC(TYPENAME, VTable)                                              \
        : public stdcolt::type_erase::TypeSize                                       \
        , public stdcolt::type_erase::FunctionCount                                  \
        , public stdcolt::type_erase::DestructorFn                                   \
        , public stdcolt::type_erase::TypeOrEmpty<                                   \
              CUSTOM.is_copy_constructible, stdcolt::type_erase::CopyConstructorFn>  \
        , public stdcolt::type_erase::TypeOrEmpty<                                   \
              CUSTOM.is_move_constructible, stdcolt::type_erase::MoveConstructorFn>  \
    {                                                                                \
      static constexpr size_t FUNCTION_COUNT =                                       \
          __COLT_MAKE_COUNT_PACK(FN1 __VA_OPT__(, ) __VA_ARGS__);                    \
      STDCOLT_FOR_EACH(                                                              \
          __COLT_MAKE_TYPE_ERASED_FN_PTRS, FN1 __VA_OPT__(, ) __VA_ARGS__)           \
      template<::NAMESPACE::CONCEPT_NAME<CUSTOM> __ABI_T>                            \
      static constexpr STDCOLT_CC(TYPENAME, VTable) make_vtable_object() noexcept    \
      {                                                                              \
        using namespace stdcolt::type_erase;                                         \
        TypeOrEmpty<CUSTOM.is_copy_constructible, CopyConstructorFn> cc{};           \
        if constexpr (CUSTOM.is_copy_constructible)                                  \
          cc.value.template make_erased_for<__ABI_T>();                              \
        TypeOrEmpty<CUSTOM.is_move_constructible, MoveConstructorFn> mc{};           \
        if constexpr (CUSTOM.is_move_constructible)                                  \
          mc.value.template make_erased_for<__ABI_T>();                              \
        DestructorFn dt{};                                                           \
        dt.template make_erased_for<__ABI_T>();                                      \
        STDCOLT_CC(TYPENAME, VTable)                                                 \
        table = {                                                                    \
            TypeSize{sizeof(__ABI_T)},                                               \
            FunctionCount{FUNCTION_COUNT},                                           \
            dt,                                                                      \
            cc,                                                                      \
            mc,                                                                      \
            STDCOLT_FOR_EACH_COMMA(                                                  \
                __COLT_MAKE_TYPE_ERASED_LAMBDAS, FN1 __VA_OPT__(, ) __VA_ARGS__)};   \
        return table;                                                                \
      }                                                                              \
      template<::NAMESPACE::CONCEPT_NAME<CUSTOM> __ABI_T>                            \
          static const STDCOLT_CC(TYPENAME, VTable) * make_vtable() noexcept         \
      {                                                                              \
        static constexpr auto table = make_vtable_object<__ABI_T>();                 \
        return &table;                                                               \
      }                                                                              \
    };                                                                               \
    template<                                                                        \
        stdcolt::type_erase::CustomizeABI CUSTOM =                                   \
            STDCOLT_CC(TYPENAME, Customization)>                                     \
    struct STDCOLT_CC(TYPENAME, Template)                                            \
    {                                                                                \
      using vtable_t =                                                               \
          const STDCOLT_CC(TYPENAME, VTable)<CUSTOM.to_customize_vtable()>;          \
      static_assert(alignof(vtable_t) > 2);                                          \
      static constexpr size_t INLINE_BUFFER_SIZE =                                   \
          stdcolt::type_erase::align_up<alignof(void*)>(CUSTOM.inline_buffer_size);  \
                                                                                     \
    private:                                                                         \
      vtable_t* _vtable;                                                             \
      union                                                                          \
      {                                                                              \
        alignas(std::max_align_t) char _buffer[INLINE_BUFFER_SIZE];                  \
        void* _heap;                                                                 \
      };                                                                             \
      inline bool on_stack() const noexcept                                          \
      {                                                                              \
        return !stdcolt::type_erase::test_lowest_bit(_vtable);                       \
      }                                                                              \
      inline bool on_heap() const noexcept                                           \
      {                                                                              \
        return !on_stack();                                                          \
      }                                                                              \
      inline const void* get_ptr() const noexcept                                    \
      {                                                                              \
        return stdcolt::type_erase::test_lowest_bit(_vtable) ? _heap : _buffer;      \
      }                                                                              \
      inline void* get_ptr() noexcept                                                \
      {                                                                              \
        return stdcolt::type_erase::test_lowest_bit(_vtable) ? _heap : _buffer;      \
      }                                                                              \
      inline vtable_t* get_vtable() const noexcept                                   \
      {                                                                              \
        return stdcolt::type_erase::clear_lowest_bit(_vtable);                       \
      }                                                                              \
                                                                                     \
    public:                                                                          \
      STDCOLT_CC(TYPENAME, Template)() = delete;                                     \
      STDCOLT_CC(TYPENAME, Template)(const STDCOLT_CC(TYPENAME, Template) &)         \
        requires(!CUSTOM.is_copy_constructible)                                      \
      = delete;                                                                      \
      STDCOLT_CC(TYPENAME, Template)(STDCOLT_CC(TYPENAME, Template) &&)              \
        requires(!CUSTOM.is_move_constructible)                                      \
      = delete;                                                                      \
      ~STDCOLT_CC(TYPENAME, Template)()                                              \
      {                                                                              \
        get_vtable()->DestructorFn::value(get_ptr());                                \
        if (on_heap())                                                               \
          CUSTOM.dealloc_fn(_heap);                                                  \
      }                                                                              \
      template<typename __ABI_T>                                                     \
        requires(::NAMESPACE::CONCEPT_NAME<__ABI_T, CUSTOM.to_customize_vtable()>)   \
                && (!std::same_as<__ABI_T, STDCOLT_CC(TYPENAME, Template)>)          \
      explicit STDCOLT_CC(TYPENAME, Template)(__ABI_T && obj)                        \
          : _vtable(vtable_t::template make_vtable<std::remove_cvref_t<__ABI_T>>())  \
      {                                                                              \
        using abi_type_t = std::remove_cvref_t<__ABI_T>;                             \
        if constexpr (sizeof(abi_type_t) <= INLINE_BUFFER_SIZE)                      \
          new (_buffer) abi_type_t(std::forward<__ABI_T>(obj));                      \
        else                                                                         \
        {                                                                            \
          auto ptr = CUSTOM.alloc_fn(sizeof(abi_type_t));                            \
          if (ptr == nullptr)                                                        \
            throw std::bad_alloc{};                                                  \
          __STDCOLT_TYPE_ERASE_TRY_RETHROW(                                          \
              (new (ptr) abi_type_t(std::forward<__ABI_T>(obj))),                    \
              CUSTOM.dealloc_fn(ptr));                                               \
          _heap   = ptr;                                                             \
          _vtable = stdcolt::type_erase::set_lowest_bit(_vtable);                    \
        }                                                                            \
      }                                                                              \
      /* COPY CONSTRUCTOR */ __COLT_MAKE_ABI_TYPE_CONSTRUCTOR(                       \
          STDCOLT_CC(TYPENAME, Template), const&, CUSTOM.is_copy_constructible,      \
          (get_vtable()                                                              \
               ->TypeOrEmpty<                                                        \
                   CUSTOM.is_copy_constructible,                                     \
                   stdcolt::type_erase::CopyConstructorFn>::value.value))            \
      /* MOVE CONSTRUCTOR */ __COLT_MAKE_ABI_TYPE_CONSTRUCTOR(                       \
          STDCOLT_CC(TYPENAME, Template), &&, CUSTOM.is_move_constructible,          \
          (get_vtable()                                                              \
               ->TypeOrEmpty<                                                        \
                   CUSTOM.is_move_constructible,                                     \
                   stdcolt::type_erase::MoveConstructorFn>::value.value))            \
      /* DEFINE METHOD CALLS WITH VTABLE */ STDCOLT_FOR_EACH_1ARG(                   \
          __COLT_MAKE_METHOD, __COLT_MAKE_METHOD_EMPTY_REQUIRE,                      \
          FN1 __VA_OPT__(, ) __VA_ARGS__)                                            \
    };                                                                               \
    using TYPENAME = STDCOLT_CC(TYPENAME, Template)<>;                               \
    template<                                                                        \
        bool IS_CONST, stdcolt::type_erase::CustomizeVTable CUSTOM =                 \
                           STDCOLT_CC(TYPENAME, VTableCustomization)>                \
    struct STDCOLT_CC(TYPENAME, RefTemplate)                                         \
    {                                                                                \
      using vtable_t = const STDCOLT_CC(TYPENAME, VTable)<CUSTOM>;                   \
                                                                                     \
    private:                                                                         \
      vtable_t* _vtable;                                                             \
      std::conditional_t<IS_CONST, const void*, void*> _object;                      \
      inline auto get_ptr() const noexcept                                           \
      {                                                                              \
        return _object;                                                              \
      }                                                                              \
      inline auto get_ptr() noexcept                                                 \
      {                                                                              \
        return _object;                                                              \
      }                                                                              \
      inline auto get_vtable() const noexcept                                        \
      {                                                                              \
        return _vtable;                                                              \
      }                                                                              \
                                                                                     \
    public:                                                                          \
      template<typename __ABI_T>                                                     \
        requires(::NAMESPACE::CONCEPT_NAME<__ABI_T, CUSTOM>) && IS_CONST             \
                    && (!std::same_as < __ABI_T,                                     \
                        STDCOLT_CC(TYPENAME, RefTemplate) < false, CUSTOM >>)        \
                    && (!std::same_as < __ABI_T,                                     \
                        STDCOLT_CC(TYPENAME, RefTemplate) < true, CUSTOM >>)         \
      explicit STDCOLT_CC(TYPENAME, RefTemplate)(const __ABI_T& obj)                 \
          : _vtable(vtable_t::template make_vtable<std::remove_cvref_t<__ABI_T>>())  \
          , _object(&obj)                                                            \
      {                                                                              \
      }                                                                              \
      template<typename __ABI_T>                                                     \
        requires(::NAMESPACE::CONCEPT_NAME<__ABI_T, CUSTOM>) && (!IS_CONST)          \
                    && (!std::same_as < __ABI_T,                                     \
                        STDCOLT_CC(TYPENAME, RefTemplate) < false, CUSTOM >>)        \
                    && (!std::same_as < __ABI_T,                                     \
                        STDCOLT_CC(TYPENAME, RefTemplate) < true, CUSTOM >>)         \
      explicit STDCOLT_CC(TYPENAME, RefTemplate)(__ABI_T & obj)                      \
          : _vtable(vtable_t::template make_vtable<std::remove_cvref_t<__ABI_T>>())  \
          , _object(&obj)                                                            \
      {                                                                              \
      }                                                                              \
      STDCOLT_CC(TYPENAME, RefTemplate)(                                             \
          const STDCOLT_CC(TYPENAME, RefTemplate) &) noexcept = default;             \
      STDCOLT_CC(TYPENAME, RefTemplate)(                                             \
          STDCOLT_CC(TYPENAME, RefTemplate) &&) noexcept = default;                  \
      STDCOLT_CC(TYPENAME, RefTemplate) & operator=(                                 \
          const STDCOLT_CC(TYPENAME, RefTemplate) &) noexcept = default;             \
      STDCOLT_CC(TYPENAME, RefTemplate) & operator=(                                 \
          STDCOLT_CC(TYPENAME, RefTemplate) &&) noexcept = default;                  \
      /* DEFINE METHOD CALLS WITH VTABLE */ STDCOLT_FOR_EACH_1ARG(                   \
          __COLT_MAKE_METHOD, __COLT_MAKE_METHOD_CONST_REQUIRE,                      \
          FN1 __VA_OPT__(, ) __VA_ARGS__)                                            \
    };                                                                               \
    using STDCOLT_CC(TYPENAME, Ref)      = STDCOLT_CC(TYPENAME, RefTemplate)<false>; \
    using STDCOLT_CC(TYPENAME, ConstRef) = STDCOLT_CC(TYPENAME, RefTemplate)<true>;  \
  }

#define STDCOLT_TYPE_ERASE_DECLARE_TEMPLATE_TYPE(                                   \
    NAMESPACE, CONCEPT_NAME, TYPENAME, CUSTOMIZATION, TEMPLATE_PARAM, FN1, ...)     \
  namespace NAMESPACE                                                               \
  {                                                                                 \
    static constexpr auto STDCOLT_CC(TYPENAME, Customization) = CUSTOMIZATION;      \
    static constexpr auto STDCOLT_CC(TYPENAME, VTableCustomization) =               \
        STDCOLT_CC(TYPENAME, Customization).to_customize_vtable();                  \
    template<                                                                       \
        typename __ABI_T,                                                           \
        STDCOLT_2D_1(TEMPLATE_PARAM) STDCOLT_2D_2(TEMPLATE_PARAM),                  \
        stdcolt::type_erase::CustomizeVTable CUSTOM =                               \
            NAMESPACE::STDCOLT_CC(TYPENAME, VTableCustomization)>                   \
    concept CONCEPT_NAME = STDCOLT_FOR_EACH_SYMBOL(                                 \
        __COLT_MAKE_REQUIRE_CLAUSE, &&, FN1 __VA_OPT__(, ) __VA_ARGS__);            \
    template<                                                                       \
        STDCOLT_2D_1(TEMPLATE_PARAM) STDCOLT_2D_2(TEMPLATE_PARAM),                  \
        stdcolt::type_erase::CustomizeVTable CUSTOM =                               \
            STDCOLT_CC(TYPENAME, VTableCustomization)>                              \
    struct STDCOLT_CC(TYPENAME, VTable)                                             \
        : public stdcolt::type_erase::TypeSize                                      \
        , public stdcolt::type_erase::FunctionCount                                 \
        , public stdcolt::type_erase::DestructorFn                                  \
        , public stdcolt::type_erase::TypeOrEmpty<                                  \
              CUSTOM.is_copy_constructible, stdcolt::type_erase::CopyConstructorFn> \
        , public stdcolt::type_erase::TypeOrEmpty<                                  \
              CUSTOM.is_move_constructible, stdcolt::type_erase::MoveConstructorFn> \
    {                                                                               \
      static constexpr size_t FUNCTION_COUNT =                                      \
          __COLT_MAKE_COUNT_PACK(FN1 __VA_OPT__(, ) __VA_ARGS__);                   \
      STDCOLT_FOR_EACH(                                                             \
          __COLT_MAKE_TYPE_ERASED_FN_PTRS, FN1 __VA_OPT__(, ) __VA_ARGS__)          \
      template<                                                                     \
          ::NAMESPACE::CONCEPT_NAME<STDCOLT_2D_2(TEMPLATE_PARAM), CUSTOM> __ABI_T>  \
      static constexpr STDCOLT_CC(TYPENAME, VTable) make_vtable_object() noexcept   \
      {                                                                             \
        using namespace stdcolt::type_erase;                                        \
        TypeOrEmpty<CUSTOM.is_copy_constructible, CopyConstructorFn> cc{};          \
        if constexpr (CUSTOM.is_copy_constructible)                                 \
          cc.value.template make_erased_for<__ABI_T>();                             \
        TypeOrEmpty<CUSTOM.is_move_constructible, MoveConstructorFn> mc{};          \
        if constexpr (CUSTOM.is_move_constructible)                                 \
          mc.value.template make_erased_for<__ABI_T>();                             \
        DestructorFn dt{};                                                          \
        dt.template make_erased_for<__ABI_T>();                                     \
        STDCOLT_CC(TYPENAME, VTable)                                                \
        table = {                                                                   \
            TypeSize{sizeof(__ABI_T)},                                              \
            FunctionCount{FUNCTION_COUNT},                                          \
            dt,                                                                     \
            cc,                                                                     \
            mc,                                                                     \
            STDCOLT_FOR_EACH_COMMA(                                                 \
                __COLT_MAKE_TYPE_ERASED_LAMBDAS, FN1 __VA_OPT__(, ) __VA_ARGS__)};  \
        return table;                                                               \
      }                                                                             \
      template<                                                                     \
          ::NAMESPACE::CONCEPT_NAME<STDCOLT_2D_2(TEMPLATE_PARAM), CUSTOM> __ABI_T>  \
          static const STDCOLT_CC(TYPENAME, VTable) * make_vtable() noexcept        \
      {                                                                             \
        static constexpr auto table = make_vtable_object<__ABI_T>();                \
        return &table;                                                              \
      }                                                                             \
    };                                                                              \
    template<                                                                       \
        STDCOLT_2D_1(TEMPLATE_PARAM) STDCOLT_2D_2(TEMPLATE_PARAM),                  \
        stdcolt::type_erase::CustomizeABI CUSTOM =                                  \
            STDCOLT_CC(TYPENAME, Customization)>                                    \
    struct STDCOLT_CC(TYPENAME, Template)                                           \
    {                                                                               \
      using vtable_t = const STDCOLT_CC(                                            \
          TYPENAME,                                                                 \
          VTable)<STDCOLT_2D_2(TEMPLATE_PARAM), CUSTOM.to_customize_vtable()>;      \
      static_assert(alignof(vtable_t) > 2);                                         \
      static constexpr size_t INLINE_BUFFER_SIZE =                                  \
          stdcolt::type_erase::align_up<alignof(void*)>(CUSTOM.inline_buffer_size); \
                                                                                    \
    private:                                                                        \
      vtable_t* _vtable;                                                            \
      union                                                                         \
      {                                                                             \
        alignas(std::max_align_t) char _buffer[INLINE_BUFFER_SIZE];                 \
        void* _heap;                                                                \
      };                                                                            \
      inline bool on_stack() const noexcept                                         \
      {                                                                             \
        return !stdcolt::type_erase::test_lowest_bit(_vtable);                      \
      }                                                                             \
      inline bool on_heap() const noexcept                                          \
      {                                                                             \
        return !on_stack();                                                         \
      }                                                                             \
      inline const void* get_ptr() const noexcept                                   \
      {                                                                             \
        return stdcolt::type_erase::test_lowest_bit(_vtable) ? _heap : _buffer;     \
      }                                                                             \
      inline void* get_ptr() noexcept                                               \
      {                                                                             \
        return stdcolt::type_erase::test_lowest_bit(_vtable) ? _heap : _buffer;     \
      }                                                                             \
      inline vtable_t* get_vtable() const noexcept                                  \
      {                                                                             \
        return stdcolt::type_erase::clear_lowest_bit(_vtable);                      \
      }                                                                             \
                                                                                    \
    public:                                                                         \
      STDCOLT_CC(TYPENAME, Template)() = delete;                                    \
      STDCOLT_CC(TYPENAME, Template)(const STDCOLT_CC(TYPENAME, Template) &)        \
        requires(!CUSTOM.is_copy_constructible)                                     \
      = delete;                                                                     \
      STDCOLT_CC(TYPENAME, Template)(STDCOLT_CC(TYPENAME, Template) &&)             \
        requires(!CUSTOM.is_move_constructible)                                     \
      = delete;                                                                     \
      ~STDCOLT_CC(TYPENAME, Template)()                                             \
      {                                                                             \
        get_vtable()->DestructorFn::value(get_ptr());                               \
        if (on_heap())                                                              \
          CUSTOM.dealloc_fn(_heap);                                                 \
      }                                                                             \
      template<typename __ABI_T>                                                    \
        requires(::NAMESPACE::CONCEPT_NAME<                                         \
                    __ABI_T, STDCOLT_2D_2(TEMPLATE_PARAM),                          \
                    CUSTOM.to_customize_vtable()>)                                  \
                && (!std::same_as<__ABI_T, STDCOLT_CC(TYPENAME, Template)>)         \
      explicit STDCOLT_CC(TYPENAME, Template)(__ABI_T && obj)                       \
          : _vtable(vtable_t::template make_vtable<std::remove_cvref_t<__ABI_T>>()) \
      {                                                                             \
        using abi_type_t = std::remove_cvref_t<__ABI_T>;                            \
        if constexpr (sizeof(abi_type_t) <= INLINE_BUFFER_SIZE)                     \
          new (_buffer) abi_type_t(std::forward<__ABI_T>(obj));                     \
        else                                                                        \
        {                                                                           \
          auto ptr = CUSTOM.alloc_fn(sizeof(abi_type_t));                           \
          if (ptr == nullptr)                                                       \
            throw std::bad_alloc{};                                                 \
          __STDCOLT_TYPE_ERASE_TRY_RETHROW(                                         \
              (new (ptr) abi_type_t(std::forward<__ABI_T>(obj))),                   \
              CUSTOM.dealloc_fn(ptr));                                              \
          _heap   = ptr;                                                            \
          _vtable = stdcolt::type_erase::set_lowest_bit(_vtable);                   \
        }                                                                           \
      }                                                                             \
      /* COPY CONSTRUCTOR */ __COLT_MAKE_ABI_TYPE_CONSTRUCTOR(                      \
          STDCOLT_CC(TYPENAME, Template), const&, CUSTOM.is_copy_constructible,     \
          (get_vtable()                                                             \
               ->TypeOrEmpty<                                                       \
                   CUSTOM.is_copy_constructible,                                    \
                   stdcolt::type_erase::CopyConstructorFn>::value.value))           \
      /* MOVE CONSTRUCTOR */ __COLT_MAKE_ABI_TYPE_CONSTRUCTOR(                      \
          STDCOLT_CC(TYPENAME, Template), &&, CUSTOM.is_move_constructible,         \
          (get_vtable()                                                             \
               ->TypeOrEmpty<                                                       \
                   CUSTOM.is_move_constructible,                                    \
                   stdcolt::type_erase::MoveConstructorFn>::value.value))           \
      /* DEFINE METHOD CALLS WITH VTABLE */ STDCOLT_FOR_EACH_1ARG(                  \
          __COLT_MAKE_METHOD, __COLT_MAKE_METHOD_EMPTY_REQUIRE,                     \
          FN1 __VA_OPT__(, ) __VA_ARGS__)                                           \
    };                                                                              \
    template<STDCOLT_2D_1(TEMPLATE_PARAM) STDCOLT_2D_2(TEMPLATE_PARAM)>             \
    using TYPENAME = STDCOLT_CC(TYPENAME, Template)<STDCOLT_2D_2(TEMPLATE_PARAM)>;  \
    template<                                                                       \
        STDCOLT_2D_1(TEMPLATE_PARAM) STDCOLT_2D_2(TEMPLATE_PARAM), bool IS_CONST,   \
        stdcolt::type_erase::CustomizeVTable CUSTOM =                               \
            STDCOLT_CC(TYPENAME, VTableCustomization)>                              \
    struct STDCOLT_CC(TYPENAME, RefTemplate)                                        \
    {                                                                               \
      using vtable_t =                                                              \
          const STDCOLT_CC(TYPENAME, VTable)<STDCOLT_2D_2(TEMPLATE_PARAM), CUSTOM>; \
                                                                                    \
    private:                                                                        \
      vtable_t* _vtable;                                                            \
      std::conditional_t<IS_CONST, const void*, void*> _object;                     \
      inline auto get_ptr() const noexcept                                          \
      {                                                                             \
        return _object;                                                             \
      }                                                                             \
      inline auto get_ptr() noexcept                                                \
      {                                                                             \
        return _object;                                                             \
      }                                                                             \
      inline auto get_vtable() const noexcept                                       \
      {                                                                             \
        return _vtable;                                                             \
      }                                                                             \
                                                                                    \
    public:                                                                         \
      template<typename __ABI_T>                                                    \
        requires(::NAMESPACE::CONCEPT_NAME<                                         \
                    __ABI_T, STDCOLT_2D_2(TEMPLATE_PARAM), CUSTOM>)                 \
                    && IS_CONST                                                     \
                    && (!std::same_as < __ABI_T,                                    \
                        STDCOLT_CC(TYPENAME, RefTemplate)                           \
                            < STDCOLT_2D_2(TEMPLATE_PARAM),                         \
                        false, CUSTOM >>)                                           \
                    && (!std::same_as < __ABI_T,                                    \
                        STDCOLT_CC(TYPENAME, RefTemplate)                           \
                            < STDCOLT_2D_2(TEMPLATE_PARAM),                         \
                        true, CUSTOM >>)                                            \
      explicit STDCOLT_CC(TYPENAME, RefTemplate)(const __ABI_T& obj)                \
          : _vtable(vtable_t::template make_vtable<std::remove_cvref_t<__ABI_T>>()) \
          , _object(&obj)                                                           \
      {                                                                             \
      }                                                                             \
      template<typename __ABI_T>                                                    \
        requires(::NAMESPACE::CONCEPT_NAME<                                         \
                    __ABI_T, STDCOLT_2D_2(TEMPLATE_PARAM), CUSTOM>)                 \
                    && (!IS_CONST)                                                  \
                    && (!std::same_as < __ABI_T,                                    \
                        STDCOLT_CC(TYPENAME, RefTemplate)                           \
                            < STDCOLT_2D_2(TEMPLATE_PARAM),                         \
                        false, CUSTOM >>)                                           \
                    && (!std::same_as < __ABI_T,                                    \
                        STDCOLT_CC(TYPENAME, RefTemplate)                           \
                            < STDCOLT_2D_2(TEMPLATE_PARAM),                         \
                        true, CUSTOM >>)                                            \
      explicit STDCOLT_CC(TYPENAME, RefTemplate)(__ABI_T & obj)                     \
          : _vtable(vtable_t::template make_vtable<std::remove_cvref_t<__ABI_T>>()) \
          , _object(&obj)                                                           \
      {                                                                             \
      }                                                                             \
      STDCOLT_CC(TYPENAME, RefTemplate)(                                            \
          const STDCOLT_CC(TYPENAME, RefTemplate) &) noexcept = default;            \
      STDCOLT_CC(TYPENAME, RefTemplate)(                                            \
          STDCOLT_CC(TYPENAME, RefTemplate) &&) noexcept = default;                 \
      STDCOLT_CC(TYPENAME, RefTemplate) & operator=(                                \
          const STDCOLT_CC(TYPENAME, RefTemplate) &) noexcept = default;            \
      STDCOLT_CC(TYPENAME, RefTemplate) & operator=(                                \
          STDCOLT_CC(TYPENAME, RefTemplate) &&) noexcept = default;                 \
      /* DEFINE METHOD CALLS WITH VTABLE */ STDCOLT_FOR_EACH_1ARG(                  \
          __COLT_MAKE_METHOD, __COLT_MAKE_METHOD_CONST_REQUIRE,                     \
          FN1 __VA_OPT__(, ) __VA_ARGS__)                                           \
    };                                                                              \
    template<STDCOLT_2D_1(TEMPLATE_PARAM) STDCOLT_2D_2(TEMPLATE_PARAM)>             \
    using STDCOLT_CC(TYPENAME, Ref) =                                               \
        STDCOLT_CC(TYPENAME, RefTemplate)<STDCOLT_2D_2(TEMPLATE_PARAM), false>;     \
    template<STDCOLT_2D_1(TEMPLATE_PARAM) STDCOLT_2D_2(TEMPLATE_PARAM)>             \
    using STDCOLT_CC(TYPENAME, ConstRef) =                                          \
        STDCOLT_CC(TYPENAME, RefTemplate)<STDCOLT_2D_2(TEMPLATE_PARAM), true>;      \
  }

/// @brief Defines a method for `STDCOLT_TYPE_ERASE_DECLARE_TYPE`
/// @warning Arguments should be provided as (TYPE, NAME) IN PARENTHESIS
/// @code{.cpp}
/// STDCOLT_TYPE_ERASE_DECLARE_TYPE(
///   test, IsBinaryFunction, BinaryFunction,
///   stdcolt::type_erase::CustomizeABI{},
///   STDCOLT_TYPE_ERASE_METHOD(double, apply, (double, a), (double, b))
/// );
/// @endcode
#define STDCOLT_TYPE_ERASE_METHOD(ret, name, ...) (ret, name, (__VA_ARGS__), (, _))
/// @brief Defines a method marked `const` for `STDCOLT_TYPE_ERASE_DECLARE_TYPE`
/// @warning Arguments should be provided as (TYPE, NAME) IN PARENTHESIS
/// @code{.cpp}
/// STDCOLT_TYPE_ERASE_DECLARE_TYPE(
///   test, IsBinaryFunction, BinaryFunction,
///   stdcolt::type_erase::CustomizeABI{},
///   STDCOLT_TYPE_ERASE_CONST_METHOD(double, apply, (double, a), (double, b))
/// );
/// @endcode
#define STDCOLT_TYPE_ERASE_CONST_METHOD(ret, name, ...) \
  (ret, name, (__VA_ARGS__), (const, _const_))

/// @brief Contains utilities used by `type_erase.h`
namespace stdcolt::type_erase
{
  /// @brief Clears the lowest bit of a pointer
  /// @tparam __ABI_T The pointer type
  /// @param p The pointer to modify
  /// @return `p` with the lowest bit set to 0
  template<typename __ABI_T>
  inline __ABI_T* clear_lowest_bit(__ABI_T* p)
  {
    auto u = reinterpret_cast<std::uintptr_t>(p);
    u &= ~static_cast<std::uintptr_t>(1);
    return reinterpret_cast<__ABI_T*>(u);
  }

  /// @brief Sets the lowest bit of a pointer
  /// @tparam __ABI_T The pointer type
  /// @param p The pointer to modify
  /// @return `p` with the lowest bit set to 1
  template<typename __ABI_T>
  inline __ABI_T* set_lowest_bit(__ABI_T* p)
  {
    auto u = reinterpret_cast<std::uintptr_t>(p);
    u |= static_cast<std::uintptr_t>(1);
    return reinterpret_cast<__ABI_T*>(u);
  }

  /// @brief Check the lowest bit of a pointer
  /// @tparam __ABI_T The pointer type
  /// @param p The pointer to modify
  /// @return The lowest bit of a pointer
  template<typename __ABI_T>
  inline bool test_lowest_bit(__ABI_T* p)
  {
    return (reinterpret_cast<std::uintptr_t>(p) & 1) != 0;
  }

  /// @brief Aligns up a value
  /// @tparam Align The alignment
  /// @param n The value to align up
  /// @return Aligned up value
  template<size_t Align>
  constexpr size_t align_up(size_t n) noexcept
  {
    static_assert(Align != 0, "alignment must be non-zero");
    static_assert((Align & (Align - 1)) == 0, "alignment must be a power of two");
    return (n + Align - 1) & ~(Align - 1);
  }
} // namespace stdcolt::type_erase

#endif // !__HG_STDCOLT_TYPE_ERASE_TYPE_ERASE
