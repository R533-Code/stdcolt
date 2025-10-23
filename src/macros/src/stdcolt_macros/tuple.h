#ifndef __HG_STDCOLT_MACROS_TUPLE
#define __HG_STDCOLT_MACROS_TUPLE

///////////////////////////////////////////////////
// __STDCOLT_*D_* (for tuples)
///////////////////////////////////////////////////

/// @brief Extract the first value of a 2D tuple (use STDCOLT_2D_1!!)
#define __STDCOLT_2D_1(X, Y) X
/// @brief Extract the first value of a macro tuple
#define STDCOLT_2D_1(TUPLE) __STDCOLT_2D_1 TUPLE
/// @brief Extract the second value of a 2D tuple (use STDCOLT_2D_2!!)
#define __STDCOLT_2D_2(X, Y) Y
/// @brief Extract the second value of a macro tuple
#define STDCOLT_2D_2(TUPLE) __STDCOLT_2D_2 TUPLE

/// @brief Extract the first value of a 3D tuple (use STDCOLT_3D_1!!)
#define __STDCOLT_3D_1(X, Y, Z) X
/// @brief Extract the first value of a macro tuple
#define STDCOLT_3D_1(TUPLE) __STDCOLT_3D_1 TUPLE
/// @brief Extract the second value of a 3D tuple (use STDCOLT_3D_2!!)
#define __STDCOLT_3D_2(X, Y, Z) Y
/// @brief Extract the second value of a macro tuple
#define STDCOLT_3D_2(TUPLE) __STDCOLT_3D_2 TUPLE
/// @brief Extract the third value of a 3D tuple (use STDCOLT_3D_3!!)
#define __STDCOLT_3D_3(X, Y, Z) Z
/// @brief Extract the third value of a macro tuple
#define STDCOLT_3D_3(TUPLE) __STDCOLT_3D_3 TUPLE

/// @brief Extract the first value of a 4D tuple (use STDCOLT_4D_1!!)
#define __STDCOLT_4D_1(X, Y, Z, A) X
/// @brief Extract the first value of a macro tuple
#define STDCOLT_4D_1(TUPLE) __STDCOLT_4D_1 TUPLE
/// @brief Extract the second value of a 4D tuple (use STDCOLT_4D_2!!)
#define __STDCOLT_4D_2(X, Y, Z, A) Y
/// @brief Extract the second value of a macro tuple
#define STDCOLT_4D_2(TUPLE) __STDCOLT_4D_2 TUPLE
/// @brief Extract the third value of a 4D tuple (use STDCOLT_4D_3!!)
#define __STDCOLT_4D_3(X, Y, Z, A) Z
/// @brief Extract the third value of a macro tuple
#define STDCOLT_4D_3(TUPLE) __STDCOLT_4D_3 TUPLE
/// @brief Extract the third value of a 4D tuple (use STDCOLT_4D_4!!)
#define __STDCOLT_4D_4(X, Y, Z, A) A
/// @brief Extract the fourth value of a macro tuple
#define STDCOLT_4D_4(TUPLE) __STDCOLT_4D_4 TUPLE

#endif // !__HG_STDCOLT_MACROS_TUPLE
