# AsobiCore — engine-agnostic protocol library used by the Unreal SDK.
#
# Builds a static library plus the doctest-driven dispatch test that
# runs on stock CI (ubuntu-24.04 / clang or g++ + cmake). No Unreal
# Engine install required.
#
# Configure:    cmake -S Source/AsobiCore -B build
# Build:        cmake --build build
# Test:         cd build && ctest --output-on-failure

cmake_minimum_required(VERSION 3.16)
project(AsobiCore CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

add_library(AsobiCore STATIC
    Private/Auth.cpp
    Private/Json.cpp
    Private/Protocol.cpp
)
target_include_directories(AsobiCore PUBLIC Public)

# Test target. doctest is a single-header lib vendored under Vendor/.
enable_testing()

add_executable(asobi_core_dispatch_test
    Tests/DispatchTest.cpp
    Tests/RpcTest.cpp
)
target_link_libraries(asobi_core_dispatch_test PRIVATE AsobiCore)
target_include_directories(asobi_core_dispatch_test PRIVATE Vendor)

# Fixtures are vendored under the AsobiSDK side of the repo. Compute
# the absolute path at configure time and inject it into the test so
# ctest works regardless of the build dir layout.
get_filename_component(_asobi_core_root "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
set(_asobi_fixture_dir "${_asobi_core_root}/../AsobiSDK/Tests/Fixtures")
target_compile_definitions(asobi_core_dispatch_test PRIVATE
    "ASOBI_CORE_FIXTURE_DIR=\"${_asobi_fixture_dir}\""
)

add_test(NAME asobi_core_dispatch
         COMMAND asobi_core_dispatch_test)

# Auth response mapping (tokens + error status/reason). Needs no fixtures.
add_executable(asobi_core_auth_test
    Tests/AuthTest.cpp
)
target_link_libraries(asobi_core_auth_test PRIVATE AsobiCore)
target_include_directories(asobi_core_auth_test PRIVATE Vendor)

add_test(NAME asobi_core_auth
         COMMAND asobi_core_auth_test)

# The outbound world.input payload shape. Needs no fixtures.
add_executable(asobi_core_world_input_test
    Tests/WorldInputTest.cpp
)
target_link_libraries(asobi_core_world_input_test PRIVATE AsobiCore)
target_include_directories(asobi_core_world_input_test PRIVATE Vendor)

add_test(NAME asobi_core_world_input
         COMMAND asobi_core_world_input_test)
