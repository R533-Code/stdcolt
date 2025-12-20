ec_add_subdirectory("${STDCOLT_SRC_DIR}/allocators")
ec_add_subdirectory("${STDCOLT_SRC_DIR}/contracts")

ec_add_library_dynamic(runtime_type OUT _runtime_type
	LINK_WITH PUBLIC stdcolt::allocators stdcolt::contracts
)