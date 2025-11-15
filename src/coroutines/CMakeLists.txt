ec_add_subdirectory("${STDCOLT_SRC_DIR}/contracts")

ec_add_library_dynamic(coroutines)
ec_get_target(coroutines _coroutines)
ec_get_target(contracts _contracts)

target_link_libraries(${_coroutines} PUBLIC ${_contracts})

ec_add_tests(test_coroutines
    TESTING_FRAMEWORK doctest
    LINK_WITH ${_coroutines}
)