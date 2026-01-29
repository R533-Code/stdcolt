ec_add_subdirectory("${STDCOLT_SRC_DIR}/allocators")
ec_add_subdirectory("${STDCOLT_SRC_DIR}/contracts")

ec_add_library_static(vocabular OUT _vocab
	LINK_WITH PUBLIC stdcolt::allocators stdcolt::contracts
)

if (STDCOLT_ENABLE_TESTS)
    ec_add_tests(test_vocabular OUT _test
        TESTING_FRAMEWORK doctest
        LINK_WITH ${_vocab}
    )
endif()