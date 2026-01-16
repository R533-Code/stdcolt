cmake_minimum_required(VERSION 3.21)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

project(stdcolt
    VERSION 0.0.0.0
    LANGUAGES CXX
)
message(STATUS "Using stdcolt v${PROJECT_VERSION}")

# ###########################
# Setup all paths
# ###########################

# The root directory of the STDCOLT project
set(STDCOLT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
cmake_path(NORMAL_PATH STDCOLT_ROOT_DIR)

# The 3rd party directory
set(STDCOLT_3RD_PARTY_DIR "${STDCOLT_ROOT_DIR}/3rd_party")
cmake_path(NORMAL_PATH STDCOLT_3RD_PARTY_DIR)

# The source directory
set(STDCOLT_SRC_DIR "${STDCOLT_ROOT_DIR}/src/")
cmake_path(NORMAL_PATH STDCOLT_SRC_DIR)

# Set the cmake installation path to `dist`
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/dist/" CACHE PATH "" FORCE)

# ###########################
# Setup Easy CMake
# ###########################
add_subdirectory("3rd_party/ECMake")

option(STDCOLT_BUILD_DOC
    "Build the stdcolt documentation"
    ON
)
option(STDCOLT_BUILD_PYTHON_BINDINGS
    "Set to true to build python bindings of the C++ code"
    ${PROJECT_IS_TOP_LEVEL}
)
option(STDCOLT_ENABLE_TESTS 
    "Set to true to build the tests of the C++ library" 
    ${PROJECT_IS_TOP_LEVEL}
)
option(STDCOLT_ENABLE_BENCHMARK 
    "Set to true to build the benchmarks of the C++ library" 
    ${PROJECT_IS_TOP_LEVEL}
)

# ###########################
# Set C++/python bindings
# ###########################
if(STDCOLT_BUILD_PYTHON_BINDINGS)
    find_package(Python 3.13 REQUIRED COMPONENTS Interpreter Development)
    ec_add_subdirectory("3rd_party/nanobind")
endif()

# ###########################
# Setup C++ testing framework
# ###########################
if (STDCOLT_ENABLE_TESTS)
    ec_add_subdirectory_static("3rd_party/doctest")
endif()

# ###########################
# Setup benchmark framework
# ###########################
if (STDCOLT_ENABLE_BENCHMARK)
    ec_add_subdirectory_static("3rd_party/nanobench")
endif()

# ###########################
# Setup stdcolt
# ###########################
ec_namespace(stdcolt)

# all libraries are prefixed with `stdcolt`

# find all the src projects
file(GLOB_RECURSE all_stdcolt RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    "src/*/CMakeLists.txt"
)

set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS
    $<$<CONFIG:Debug>:STDCOLT_DEBUG>
)

foreach(subdir ${all_stdcolt})
    # strip CMakeLists.txt from name
    cmake_path(GET subdir PARENT_PATH subdir)

    # and skip 3rd_party dependencies
    if(subdir MATCHES ".*3rd_party.*")
        continue()
    endif()

    ec_add_subdirectory(${subdir})
endforeach()

ec_endnamespace(stdcolt)

# #################
# DOXYGEN
# #################
if(STDCOLT_BUILD_DOC)
    find_package(Doxygen)

    if(DOXYGEN_FOUND)
        message(STATUS "Found Doxygen executable.")

        # set input and output files
        set(DOXYGEN_IN "${CMAKE_CURRENT_SOURCE_DIR}/resources/Doxyfile.in")
        set(DOXYGEN_OUT "${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.out")

        message(STATUS "Configuring Doxygen input file...\n")

        # request to configure the file
        configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT})

        add_custom_target(GENERATE_DOCS
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
        )
    else()
        message(WARNING "Doxygen was not found! Doxygen need to be installed to generate the documentation!\n")
    endif()
endif()
