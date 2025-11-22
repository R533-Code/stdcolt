ec_add_subdirectory("${STDCOLT_SRC_DIR}/contracts")
ec_add_subdirectory("${STDCOLT_3RD_PARTY_DIR}/concurrentqueue")

ec_add_library_dynamic(coroutines)
ec_get_target(coroutines _coroutines)
ec_get_target(contracts _contracts)

target_link_libraries(${_coroutines} PUBLIC ${_contracts} concurrentqueue)

ec_add_tests(test_coroutines
    TESTING_FRAMEWORK doctest
    LINK_WITH ${_coroutines}
)
ec_get_target(test_coroutines _test)

ec_copy_dependencies(${_coroutines})
ec_copy_dependencies(${_test})

if(STDCOLT_BUILD_PYTHON_BINDINGS)
    ec_add_python_bindings(pycolt_coroutines
        BINDING_FRAMEWORK nanobind
        LINK_WITH PUBLIC ${_coroutines}
    )
endif()