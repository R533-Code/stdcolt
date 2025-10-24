ec_add_subdirectory("${STDCOLT_SRC_DIR}/macros")
ec_add_subdirectory("${STDCOLT_SRC_DIR}/contracts")
ec_add_library_dynamic(type_erase)

ec_get_target(type_erase _type_erase)
ec_get_target(macros _macros)
ec_get_target(contracts _contracts)

target_link_libraries(${_type_erase} PUBLIC ${_macros} ${_contracts})

ec_add_tests(test_type_erase
    TESTING_FRAMEWORK doctest
    LINK_WITH ${_type_erase}
)