ec_add_subdirectory("${STDCOLT_SRC_DIR}/allocators")
ec_add_subdirectory("${STDCOLT_SRC_DIR}/contracts")

ec_add_library_dynamic(runtime_type OUT _runtime_type
	LINK_WITH PUBLIC stdcolt::allocators stdcolt::contracts
)

if (STDCOLT_ENABLE_TESTS)
    ec_add_tests(test_runtime_type OUT _test
        TESTING_FRAMEWORK doctest
        LINK_WITH ${_runtime_type}
    )
endif()

if(STDCOLT_BUILD_PYTHON_BINDINGS)
    ec_add_python_bindings(pycolt_ext_runtime_type
        BINDING_FRAMEWORK nanobind
        LINK_WITH PUBLIC ${_runtime_type}
    )
endif()