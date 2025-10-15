function(enable_simd_for TARGET_NAME)
  if (MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE
      /arch:AVX2 # /arch:AVX512
    )
  elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    # x86/x86_64: enable wide set of intrinsics
    target_compile_options(${TARGET_NAME} PRIVATE
      -mavx2 -mfma -mbmi -mbmi2 -maes -mpclmul -mvaes -mvpclmulqdq
      -mavx512f -mavx512bw -mavx512dq -mavx512cd -mavx512vl -mavx512vnni)
  endif()

  # ARM/AArch64 (GCC/Clang). Only apply on ARM targets.
  if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
      target_compile_options(${TARGET_NAME} PRIVATE
        -march=armv8.6-a+simd+crypto+dotprod+i8mm+bf16+sve+sve2)
    endif()
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(armv7|armv8l)$")
    target_compile_options(${TARGET_NAME} PRIVATE -mfpu=neon -mfloat-abi=hard)
  endif()

  # AppleClang on Apple Silicon: NEON is default
  if (APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$" AND CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    # maybe add other features?
  endif()
endfunction()

ec_add_library_dynamic(simd)
ec_get_target(simd _simd)
enable_simd_for(${_simd})

ec_add_executable(bench_simd ROOT_DIR "benchmarks")
ec_get_target(bench_simd _bench_simd)
target_link_libraries(${_bench_simd} PUBLIC nanobench ${_simd})

ec_add_tests(test_simd
    TESTING_FRAMEWORK doctest
    LINK_WITH ${_simd}
)