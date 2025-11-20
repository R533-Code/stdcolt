ec_add_subdirectory("${STDCOLT_SRC_DIR}/contracts")

ec_add_library_dynamic(allocators)
ec_get_target(allocators _alloc)
ec_get_target(contracts _contracts)

target_link_libraries(${_alloc} PUBLIC ${_contracts})

ec_add_tests(test_allocators
    TESTING_FRAMEWORK doctest
    LINK_WITH ${_alloc}
)
ec_get_target(test_allocators _test)

ec_copy_dependencies(${_alloc})
ec_copy_dependencies(${_test})