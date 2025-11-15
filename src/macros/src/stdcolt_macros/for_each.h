/*****************************************************************/ /**
 * @file   for_each.h
 * @brief  Contains `STDCOLT_FOR_EACH_*` macros.
 * 
 * @author Raphael Dib Nehme
 * @date   November 2025
 *********************************************************************/
#ifndef __HG_STDCOLT_MACROS_FOR_EACH
#define __HG_STDCOLT_MACROS_FOR_EACH

#include <stdcolt_macros/compiler.h>

/// @brief Concatenate internal (use STDCOLT_CC!!)
#define __DETAILS__STDCOLT_CC2(x, y) x##y
/// @brief Concatenate two values
#define STDCOLT_CC(x, y) __DETAILS__STDCOLT_CC2(x, y)
/// @brief Pair of ()
#define __DETAILS__STDCOLT_PARENS ()

///////////////////////////////////////////////////
// STDCOLT_FOR_EACH_*
///////////////////////////////////////////////////

/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_EXPAND(...)                   \
  __DETAILS__STDCOLT_EXPAND4(__DETAILS__STDCOLT_EXPAND4( \
      __DETAILS__STDCOLT_EXPAND4(__DETAILS__STDCOLT_EXPAND4(__VA_ARGS__))))
/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_EXPAND4(...)                  \
  __DETAILS__STDCOLT_EXPAND3(__DETAILS__STDCOLT_EXPAND3( \
      __DETAILS__STDCOLT_EXPAND3(__DETAILS__STDCOLT_EXPAND3(__VA_ARGS__))))
/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_EXPAND3(...)                  \
  __DETAILS__STDCOLT_EXPAND2(__DETAILS__STDCOLT_EXPAND2( \
      __DETAILS__STDCOLT_EXPAND2(__DETAILS__STDCOLT_EXPAND2(__VA_ARGS__))))
/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_EXPAND2(...)                  \
  __DETAILS__STDCOLT_EXPAND1(__DETAILS__STDCOLT_EXPAND1( \
      __DETAILS__STDCOLT_EXPAND1(__DETAILS__STDCOLT_EXPAND1(__VA_ARGS__))))
/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_EXPAND1(...) __VA_ARGS__

/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_FOR_EACH_HELPER_SYMBOL(macro, symbol, a1, ...) \
  macro(a1) __VA_OPT__(symbol) __VA_OPT__(                                \
      __DETAILS__STDCOLT_FOR_EACH_AGAIN_SYMBOL __DETAILS__STDCOLT_PARENS( \
          macro, symbol, __VA_ARGS__))
/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_FOR_EACH_AGAIN_SYMBOL() \
  __DETAILS__STDCOLT_FOR_EACH_HELPER_SYMBOL

/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_FOR_EACH_HELPER_COMMA(macro, a1, ...)                    \
  macro(a1) __VA_OPT__(, )                                                          \
      __VA_OPT__(__DETAILS__STDCOLT_FOR_EACH_AGAIN_COMMA __DETAILS__STDCOLT_PARENS( \
          macro, __VA_ARGS__))
/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_FOR_EACH_AGAIN_COMMA() \
  __DETAILS__STDCOLT_FOR_EACH_HELPER_COMMA

/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_FOR_EACH_HELPER(macro, a1, ...)                          \
  macro(a1) __VA_OPT__(__DETAILS__STDCOLT_FOR_EACH_AGAIN __DETAILS__STDCOLT_PARENS( \
      macro, __VA_ARGS__))
/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_FOR_EACH_AGAIN() __DETAILS__STDCOLT_FOR_EACH_HELPER

/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_FOR_EACH_HELPER_1ARG(macro, arg, a1, ...)               \
  macro(arg, a1)                                                                   \
      __VA_OPT__(__DETAILS__STDCOLT_FOR_EACH_AGAIN_1ARG __DETAILS__STDCOLT_PARENS( \
          macro, arg, __VA_ARGS__))

/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_FOR_EACH_AGAIN_1ARG() \
  __DETAILS__STDCOLT_FOR_EACH_HELPER_1ARG

/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_FOR_EACH_HELPER_2ARG(macro, arg1, arg2, a1, ...)        \
  macro(arg1, arg2, a1)                                                            \
      __VA_OPT__(__DETAILS__STDCOLT_FOR_EACH_AGAIN_2ARG __DETAILS__STDCOLT_PARENS( \
          macro, arg1, arg2, __VA_ARGS__))

/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_FOR_EACH_AGAIN_2ARG() \
  __DETAILS__STDCOLT_FOR_EACH_HELPER_2ARG

/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_FOR_EACH_HELPER_3ARG(macro, arg1, arg2, arg3, a1, ...)  \
  macro(arg1, arg2, arg3, a1)                                                      \
      __VA_OPT__(__DETAILS__STDCOLT_FOR_EACH_AGAIN_3ARG __DETAILS__STDCOLT_PARENS( \
          macro, arg1, arg2, arg3, __VA_ARGS__))

/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_FOR_EACH_AGAIN_3ARG() \
  __DETAILS__STDCOLT_FOR_EACH_HELPER_3ARG

/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_FOR_EACH_HELPER_4ARG(                                   \
    macro, arg1, arg2, arg3, arg4, a1, ...)                                        \
  macro(arg1, arg2, arg3, arg4, a1)                                                \
      __VA_OPT__(__DETAILS__STDCOLT_FOR_EACH_AGAIN_4ARG __DETAILS__STDCOLT_PARENS( \
          macro, arg1, arg2, arg3, arg4, __VA_ARGS__))

/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_FOR_EACH_AGAIN_4ARG() \
  __DETAILS__STDCOLT_FOR_EACH_HELPER_4ARG

///////////////////////////////////////////////////
// STDCOLT_FOR_EACH_*_2
///////////////////////////////////////////////////

/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_EXPAND_2(...)                   \
  __DETAILS__STDCOLT_EXPAND42(__DETAILS__STDCOLT_EXPAND42( \
      __DETAILS__STDCOLT_EXPAND42(__DETAILS__STDCOLT_EXPAND42(__VA_ARGS__))))
/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_EXPAND42(...)                   \
  __DETAILS__STDCOLT_EXPAND32(__DETAILS__STDCOLT_EXPAND32( \
      __DETAILS__STDCOLT_EXPAND32(__DETAILS__STDCOLT_EXPAND32(__VA_ARGS__))))
/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_EXPAND32(...)                   \
  __DETAILS__STDCOLT_EXPAND22(__DETAILS__STDCOLT_EXPAND22( \
      __DETAILS__STDCOLT_EXPAND22(__DETAILS__STDCOLT_EXPAND22(__VA_ARGS__))))
/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_EXPAND22(...)                   \
  __DETAILS__STDCOLT_EXPAND12(__DETAILS__STDCOLT_EXPAND12( \
      __DETAILS__STDCOLT_EXPAND12(__DETAILS__STDCOLT_EXPAND12(__VA_ARGS__))))
/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_EXPAND12(...) __VA_ARGS__

/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_FOR_EACH_HELPER_SYMBOL2(macro, symbol, a1, ...) \
  macro(a1) __VA_OPT__(symbol) __VA_OPT__(                                 \
      __DETAILS__STDCOLT_FOR_EACH_AGAIN_SYMBOL2 __DETAILS__STDCOLT_PARENS( \
          macro, symbol, __VA_ARGS__))
/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_FOR_EACH_AGAIN_SYMBOL2() \
  __DETAILS__STDCOLT_FOR_EACH_HELPER_SYMBOL2

/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_FOR_EACH_HELPER_COMMA2(macro, a1, ...)         \
  macro(a1) __VA_OPT__(, ) __VA_OPT__(                                    \
      __DETAILS__STDCOLT_FOR_EACH_AGAIN_COMMA2 __DETAILS__STDCOLT_PARENS( \
          macro, __VA_ARGS__))
/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_FOR_EACH_AGAIN_COMMA2() \
  __DETAILS__STDCOLT_FOR_EACH_HELPER_COMMA2

/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_FOR_EACH_HELPER2(macro, a1, ...)                    \
  macro(a1)                                                                    \
      __VA_OPT__(__DETAILS__STDCOLT_FOR_EACH_AGAIN2 __DETAILS__STDCOLT_PARENS( \
          macro, __VA_ARGS__))
/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_FOR_EACH_AGAIN2() __DETAILS__STDCOLT_FOR_EACH_HELPER2

/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_FOR_EACH_HELPER_1ARG2(macro, arg, a1, ...)               \
  macro(arg, a1)                                                                    \
      __VA_OPT__(__DETAILS__STDCOLT_FOR_EACH_AGAIN_1ARG2 __DETAILS__STDCOLT_PARENS( \
          macro, arg, __VA_ARGS__))

/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_FOR_EACH_AGAIN_1ARG2() \
  __DETAILS__STDCOLT_FOR_EACH_HELPER_1ARG2

/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_FOR_EACH_HELPER_2ARG2(macro, arg1, arg2, a1, ...)        \
  macro(arg1, arg2, a1)                                                             \
      __VA_OPT__(__DETAILS__STDCOLT_FOR_EACH_AGAIN_2ARG2 __DETAILS__STDCOLT_PARENS( \
          macro, arg1, arg2, __VA_ARGS__))

/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_FOR_EACH_AGAIN_2ARG2() \
  __DETAILS__STDCOLT_FOR_EACH_HELPER_2ARG2

/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_FOR_EACH_HELPER_3ARG2(macro, arg1, arg2, arg3, a1, ...)  \
  macro(arg1, arg2, arg3, a1)                                                       \
      __VA_OPT__(__DETAILS__STDCOLT_FOR_EACH_AGAIN_3ARG2 __DETAILS__STDCOLT_PARENS( \
          macro, arg1, arg2, arg3, __VA_ARGS__))

/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_FOR_EACH_AGAIN_3ARG2() \
  __DETAILS__STDCOLT_FOR_EACH_HELPER_3ARG2

/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_FOR_EACH_HELPER_4ARG2(                                   \
    macro, arg1, arg2, arg3, arg4, a1, ...)                                         \
  macro(arg1, arg2, arg3, arg4, a1)                                                 \
      __VA_OPT__(__DETAILS__STDCOLT_FOR_EACH_AGAIN_4ARG2 __DETAILS__STDCOLT_PARENS( \
          macro, arg1, arg2, arg3, arg4, __VA_ARGS__))

/// @brief Helper for STDCOLT_FOR_EACH_*
#define __DETAILS__STDCOLT_FOR_EACH_AGAIN_4ARG2() \
  __DETAILS__STDCOLT_FOR_EACH_HELPER_4ARG2

///////////////////////////////////////////////////
// __STDCOLT_deparen
///////////////////////////////////////////////////

// Helpers for __STDCOLT_deparen
#define __STDCOLT_ISH(...)  __STDCOLT_ISH __VA_ARGS__
#define __STDCOLT_ESC(...)  __STDCOLT_ESC_(__VA_ARGS__)
#define __STDCOLT_ESC_(...) __STDCOLT_VAN##__VA_ARGS__
#define __STDCOLT_VAN__STDCOLT_ISH

/// @brief Applies 'macro' on each arguments
#define STDCOLT_FOR_EACH(macro, ...)    \
  __VA_OPT__(__DETAILS__STDCOLT_EXPAND( \
      __DETAILS__STDCOLT_FOR_EACH_HELPER(macro, __VA_ARGS__)))

/// @brief Applies 'macro' on each arguments, separating all by 'symbol' (without ending with it).
#define STDCOLT_FOR_EACH_SYMBOL(macro, symbol, ...) \
  __VA_OPT__(__DETAILS__STDCOLT_EXPAND(             \
      __DETAILS__STDCOLT_FOR_EACH_HELPER_SYMBOL(macro, symbol, __VA_ARGS__)))

/// @brief Applies 'macro' on each arguments, separating all by ',' (without ending with it).
/// This is VERY useful to expand function calls.
#define STDCOLT_FOR_EACH_COMMA(macro, ...) \
  __VA_OPT__(__DETAILS__STDCOLT_EXPAND(    \
      __DETAILS__STDCOLT_FOR_EACH_HELPER_COMMA(macro, __VA_ARGS__)))

/// @brief Applies 'macro' on each arguments, invoking 'macro(arg1, arg2, arg3, arg4, <ARG>)'
#define STDCOLT_FOR_EACH_4ARG(macro, arg1, arg2, arg3, arg4, ...)               \
  __VA_OPT__(__DETAILS__STDCOLT_EXPAND(__DETAILS__STDCOLT_FOR_EACH_HELPER_4ARG( \
      macro, arg1, arg2, arg3, arg4, __VA_ARGS__)))

/// @brief Applies 'macro' on each arguments, invoking 'macro(arg1, arg2, arg3, <ARG>)'
#define STDCOLT_FOR_EACH_3ARG(macro, arg1, arg2, arg3, ...)                     \
  __VA_OPT__(__DETAILS__STDCOLT_EXPAND(__DETAILS__STDCOLT_FOR_EACH_HELPER_3ARG( \
      macro, arg1, arg2, arg3, __VA_ARGS__)))

/// @brief Applies 'macro' on each arguments, invoking 'macro(arg1, arg2, <ARG>)'
#define STDCOLT_FOR_EACH_2ARG(macro, arg1, arg2, ...) \
  __VA_OPT__(__DETAILS__STDCOLT_EXPAND(               \
      __DETAILS__STDCOLT_FOR_EACH_HELPER_2ARG(macro, arg1, arg2, __VA_ARGS__)))

/// @brief Applies 'macro' on each arguments, invoking 'macro(arg, <ARG>)'
#define STDCOLT_FOR_EACH_1ARG(macro, arg, ...) \
  __VA_OPT__(__DETAILS__STDCOLT_EXPAND(        \
      __DETAILS__STDCOLT_FOR_EACH_HELPER_1ARG(macro, arg, __VA_ARGS__)))

// STDCOLT_FOR_EACH2 for nested loops vvv

/// @brief Applies 'macro' on each arguments
#define STDCOLT_FOR_EACH2(macro, ...)     \
  __VA_OPT__(__DETAILS__STDCOLT_EXPAND_2( \
      __DETAILS__STDCOLT_FOR_EACH_HELPER2(macro, __VA_ARGS__)))

/// @brief Applies 'macro' on each arguments, separating all by 'symbol' (without ending with it).
#define STDCOLT_FOR_EACH_SYMBOL2(macro, symbol, ...) \
  __VA_OPT__(__DETAILS__STDCOLT_EXPAND_2(            \
      __DETAILS__STDCOLT_FOR_EACH_HELPER_SYMBOL2(macro, symbol, __VA_ARGS__)))

/// @brief Applies 'macro' on each arguments, separating all by ',' (without ending with it).
/// This is VERY useful to expand function calls.
#define STDCOLT_FOR_EACH_COMMA2(macro, ...) \
  __VA_OPT__(__DETAILS__STDCOLT_EXPAND_2(   \
      __DETAILS__STDCOLT_FOR_EACH_HELPER_COMMA2(macro, __VA_ARGS__)))

/// @brief Applies 'macro' on each arguments, invoking 'macro(arg1, arg2, arg3, arg4, <ARG>)'
#define STDCOLT_FOR_EACH_4ARG2(macro, arg1, arg2, arg3, arg4, ...)                 \
  __VA_OPT__(__DETAILS__STDCOLT_EXPAND_2(__DETAILS__STDCOLT_FOR_EACH_HELPER_4ARG2( \
      macro, arg1, arg2, arg3, arg4, __VA_ARGS__)))

/// @brief Applies 'macro' on each arguments, invoking 'macro(arg1, arg2, arg3, <ARG>)'
#define STDCOLT_FOR_EACH_3ARG2(macro, arg1, arg2, arg3, ...)                       \
  __VA_OPT__(__DETAILS__STDCOLT_EXPAND_2(__DETAILS__STDCOLT_FOR_EACH_HELPER_3ARG2( \
      macro, arg1, arg2, arg3, __VA_ARGS__)))

/// @brief Applies 'macro' on each arguments, invoking 'macro(arg1, arg2, <ARG>)'
#define STDCOLT_FOR_EACH_2ARG2(macro, arg1, arg2, ...) \
  __VA_OPT__(__DETAILS__STDCOLT_EXPAND_2(              \
      __DETAILS__STDCOLT_FOR_EACH_HELPER_2ARG2(macro, arg1, arg2, __VA_ARGS__)))

/// @brief Applies 'macro' on each arguments, invoking 'macro(arg, <ARG>)'
#define STDCOLT_FOR_EACH_1ARG2(macro, arg, ...) \
  __VA_OPT__(__DETAILS__STDCOLT_EXPAND_2(       \
      __DETAILS__STDCOLT_FOR_EACH_HELPER_1ARG2(macro, arg, __VA_ARGS__)))

/// @brief If x is wrapped in parenthesis, removes them
#define STDCOLT_DEPAREN(x) __STDCOLT_ESC(__STDCOLT_ISH x)
/// @brief If x is wrapped in parenthesis, does nothing, else adds parenthesis
#define STDCOLT_ADDPAREN(x) (__STDCOLT_ESC(__STDCOLT_ISH x))

#endif // !__HG_STDCOLT_MACROS_FOR_EACH
