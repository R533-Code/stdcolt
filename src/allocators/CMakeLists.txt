ec_add_subdirectory("${STDCOLT_SRC_DIR}/contracts")
ec_get_target(contracts _contracts)

ec_add_library_dynamic(allocators OUT _alloc
    LINK_WITH PUBLIC ${_contracts}
)

ec_add_tests(test_allocators OUT _test
    TESTING_FRAMEWORK doctest
    LINK_WITH ${_alloc}
)

ec_copy_dependencies(${_alloc})
ec_copy_dependencies(${_test})