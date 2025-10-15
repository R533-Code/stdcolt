ec_add_subdirectory("3rd_party/cpptrace")

ec_add_library_dynamic(contracts)
ec_get_target(contracts _contracts)

target_link_libraries(${_contracts} PRIVATE cpptrace::cpptrace)

ec_add_tests(test-contracts
    TESTING_FRAMEWORK doctest
    LINK_WITH ${_contracts}
)