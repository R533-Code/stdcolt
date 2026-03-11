option(STDCOLT_ENABLE_TRACING
    "Enable tracing of stdcolt"
    ON
)

ec_add_library_static(tracing OUT _tracing)

if(${STDCOLT_ENABLE_TRACING})
    set(TRACY_ENABLE ON CACHE BOOL "" FORCE)
    ec_add_subdirectory_dynamic("3rd_party/tracy")
    set_target_properties(TracyClient PROPERTIES OUTPUT_NAME libtracy PREFIX "")
    target_include_directories(${_tracing} PUBLIC
        "3rd_party/tracy/public/tracy"
    )
    target_compile_definitions(${_tracing} PUBLIC STDCOLT_ENABLE_TRACING TRACY_ENABLE)
    target_link_directories(${_tracing} PUBLIC Tracy::TracyClient)
else()
    message(STATUS "Skipping library 'Tracy'. Set STDCOLT_ENABLE_TRACING to ON to enable tracing.")
endif()

if(${STDCOLT_BUILD_TRACY_PROFILER})
    message(STATUS "Build of 'tracy-profiler' is enabled.")
    ec_add_subdirectory("3rd_party/tracy/profiler")
else()
    message(STATUS "Skipping build of 'tracy-profiler'. Set STDCOLT_BUILD_TRACY_PROFILER to ON to build it.")
endif()