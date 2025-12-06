set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
ec_add_subdirectory("3rd_party/libsimdpp")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/libsimdpp/cmake")
include(SimdppMultiarch)

simdpp_get_compilable_archs(COMPILABLE_ARCHS)
simdpp_multiarch(STDCOLT_SIMD_OBJS
    "simd.cpp"
    ${COMPILABLE_ARCHS}
)

ec_add_library_dynamic(simd OUT _simd
    NO_CONFORMANT_PREPROCESSOR_MSVC
)

target_include_directories(${_simd} PUBLIC "3rd_party/libsimdpp")
# fix sources of simd target:
target_sources(${_simd} PRIVATE ${STDCOLT_SIMD_OBJS})

ec_add_tests(test_simd OUT _test
    TESTING_FRAMEWORK doctest
    LINK_WITH ${_simd}
)

ec_copy_dependencies(${_simd})
ec_copy_dependencies(${_test})