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
set(STDCOLT_ROOT_DIR "${CMAKE_SOURCE_DIR}")
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

# ###########################
# Setup C++ testing framework
# ###########################
ec_add_subdirectory_static("3rd_party/doctest")

# ###########################
# Setup benchmark framework
# ###########################
ec_add_subdirectory_static("3rd_party/nanobench")

# ###########################
# Setup stdcolt
# ###########################
ec_namespace(stdcolt)
# all cppcore are prefixed with `stdcolt`

# find all the cppcore projects
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
    if (subdir MATCHES ".*3rd_party.*")
        continue()
    endif()
    ec_add_subdirectory(${subdir})
endforeach()

ec_endnamespace(stdcolt)