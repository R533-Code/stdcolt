ec_add_subdirectory("${STDCOLT_SRC_DIR}/contracts")
ec_add_subdirectory("${STDCOLT_3RD_PARTY_DIR}/concurrentqueue")
ec_get_target(contracts _contracts)

ec_add_library_dynamic(coroutines OUT _coroutines
    LINK_WITH PUBLIC ${_contracts} concurrentqueue
)

if (STDCOLT_ENABLE_TESTS)
    ec_add_tests(test_coroutines OUT _test
        TESTING_FRAMEWORK doctest
        LINK_WITH ${_coroutines}
    )
endif()

if(STDCOLT_BUILD_PYTHON_BINDINGS)
    ec_add_python_bindings(pycolt_coroutines
        BINDING_FRAMEWORK nanobind
        LINK_WITH PUBLIC ${_coroutines}
    )
endif()