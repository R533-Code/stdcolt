ec_add_subdirectory("3rd_party/cpptrace")
ec_add_subdirectory("${STDCOLT_SRC_DIR}/debugging")
ec_get_target(debugging _debugging)

ec_add_library_dynamic(contracts OUT _contracts
    LINK_WITH PRIVATE cpptrace::cpptrace PUBLIC ${_debugging}
)

ec_add_tests(test_contracts OUT _test
    TESTING_FRAMEWORK doctest
    LINK_WITH ${_contracts}
)

ec_copy_dependencies(${_contracts})
ec_copy_dependencies(${_test})