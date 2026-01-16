ec_add_subdirectory("${STDCOLT_SRC_DIR}/macros")
ec_add_subdirectory("${STDCOLT_SRC_DIR}/contracts")

ec_get_target(macros _macros)
ec_get_target(contracts _contracts)

ec_add_library_dynamic(type_erase OUT _type_erase
    LINK_WITH PUBLIC ${_macros} ${_contracts}
)

if (STDCOLT_ENABLE_TESTS)
    ec_add_tests(test_type_erase OUT _test
        TESTING_FRAMEWORK doctest
        LINK_WITH ${_type_erase}
    )
endif()
