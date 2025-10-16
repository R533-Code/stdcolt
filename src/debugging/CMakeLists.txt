ec_add_subdirectory("${STDCOLT_SRC_DIR}/macros")
ec_add_library_dynamic(debugging)

ec_get_target(debugging _debugging)
ec_get_target(macros _macros)

target_link_libraries(${_debugging} PRIVATE ${_macros})
