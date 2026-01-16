ec_add_subdirectory("${STDCOLT_SRC_DIR}/macros")
ec_get_target(macros _macros)

ec_add_library_dynamic(debugging OUT _debugging
    LINK_WITH PUBLIC ${_macros}
)