ec_add_subdirectory("3rd_party/cpptrace")
ec_add_subdirectory("${STDCOLT_SRC_DIR}/debugging")

ec_add_library_dynamic(contracts)
ec_get_target(contracts _contracts)
ec_get_target(debugging _debugging)

target_link_libraries(${_contracts}
    PRIVATE cpptrace::cpptrace
    PUBLIC ${_debugging}
)

ec_add_tests(test-contracts
    TESTING_FRAMEWORK doctest
    LINK_WITH ${_contracts}
)