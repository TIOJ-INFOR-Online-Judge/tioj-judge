cmake_minimum_required(VERSION 3.10)

project(tioj_judge VERSION 0.1 LANGUAGES CXX)
set(SOVERSION 0)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS_DEBUG "-Og -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "ON")

# options
option(TIOJ_BUILD_TESTS "Build test programs" ON)
option(TIOJ_INSTALL_LIBTIOJ "Install libtioj" ON)
option(TIOJ_INSTALL_DEPENDENCIES "Install third-party public header files used by libtioj (nlohmann_json and cjail)" ON)

# force build with static because sandbox-exec
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")

# configs
set(TIOJ_DATA_DIR "/var/lib/tioj-judge/" CACHE PATH "Path of default testdata & scoring program location")

add_compile_definitions(TIOJ_DATA_DIR="${TIOJ_DATA_DIR}")

# setup dependencies
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CJAIL_BUILD_CLI OFF)
set(CJAIL_BUILD_TESTS OFF)
set(HTTPLIB_REQUIRE_OPENSSL ON)
set(HTTPLIB_REQUIRE_ZLIB ON)
set(HTTPLIB_COMPILE ON)
set(JSON_MultipleHeaders ON)
set(JSON_Install ON)
set(BUILD_TESTING OFF) # for tortellini

include(FetchContentExclude)
FetchContent_Declare(
  cjail
  GIT_REPOSITORY "https://github.com/adrien1018/cjail.git"
  GIT_TAG "66319a18ab0c23af11e69e042a2e9bd978789962"
)
FetchContent_Declare(
  spdlog
  URL "https://github.com/gabime/spdlog/archive/refs/tags/v1.10.0.zip"
)
FetchContent_Declare(
  fmt
  URL "https://github.com/fmtlib/fmt/archive/refs/tags/8.1.1.zip"
)
FetchContent_Declare(
  json
  URL "https://github.com/nlohmann/json/archive/refs/tags/v3.11.2.zip"
)
FetchContent_Declare(
  ini
  GIT_REPOSITORY "https://github.com/Qix-/tortellini.git"
  GIT_TAG "0c9a2b79f6580d44bd7606f30668a5e0ba8326ae"
)
FetchContent_Declare(
  httplib
  URL "https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.12.0.zip"
)
FetchContent_Declare(
  websocketpp
  URL "https://github.com/zaphoyd/websocketpp/archive/refs/tags/0.8.2.zip"
)
FetchContent_Declare(
  sqlite_orm
  URL "https://github.com/fnc12/sqlite_orm/archive/refs/tags/v1.8.1.zip"
)
FetchContent_Declare(
  argparse
  URL "https://github.com/p-ranav/argparse/archive/refs/tags/v2.9.zip"
)

if(TIOJ_INSTALL_LIBTIOJ AND TIOJ_INSTALL_DEPENDENCIES)
  FetchContent_MakeAvailable_Include(cjail json)
else()
  FetchContent_MakeAvailable_Exclude(cjail json)
endif()

find_package(OpenSSL REQUIRED)
find_package(zstd REQUIRED)
FetchContent_MakeAvailable_Exclude(spdlog fmt ini httplib websocketpp sqlite_orm argparse)

# default scoring
add_executable(default-scoring "tools/default-scoring.cpp")
target_link_libraries(default-scoring nlohmann_json::nlohmann_json)
install(TARGETS default-scoring DESTINATION "${TIOJ_DATA_DIR}/")
# sandbox exec
# static link it to minimize memory usage; this will lead to some warnings about libc, but it is okay
add_executable(sandbox-exec "src/tioj/sandbox_main.cpp" "src/tioj/sandbox.cpp" "src/tioj/sandbox.h")
target_link_libraries(sandbox-exec CJail::libcjail)
target_compile_options(sandbox-exec PUBLIC -Os -static) # note that later flags will override previous ones
target_link_options(sandbox-exec PUBLIC -static -pthread)
install(TARGETS sandbox-exec DESTINATION "${TIOJ_DATA_DIR}/")

# headers for judge
file(DOWNLOAD "https://raw.githubusercontent.com/nlohmann/json/v3.10.5/single_include/nlohmann/json.hpp"
  "${PROJECT_BINARY_DIR}/judge-headers/nlohmann/json.hpp")
file(DOWNLOAD "https://raw.githubusercontent.com/MikeMirzayanov/testlib/6daa79fdc9c2b113be8d40f828e3dc81e67ce33d/testlib.h"
  "${PROJECT_BINARY_DIR}/judge-headers/testlib.h")
install(DIRECTORY "${PROJECT_BINARY_DIR}/judge-headers" DESTINATION "${TIOJ_DATA_DIR}")

# library
# required for any program that uses libtioj
add_subdirectory(src)

# judge
file(GLOB JUDGE_SRC "src/*.cpp" "src/*.h")
add_executable(tioj-judge ${JUDGE_SRC})
target_link_libraries(
    tioj-judge libtioj spdlog::spdlog fmt::fmt tortellini
    httplib::httplib websocketpp ${OPENSSL_LIBRARIES} zstd
    sqlite_orm::sqlite_orm argparse::argparse)
install(TARGETS tioj-judge DESTINATION bin)

# testing
if(TIOJ_BUILD_TESTS)
  enable_testing()
  FetchContent_Declare(
    googletest
    URL "https://github.com/google/googletest/archive/refs/tags/release-1.11.0.zip"
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable_Exclude(googletest)

  file(GLOB TEST_SRC "test/*.cpp" "test/*.h")
  add_executable(judge-test ${TEST_SRC})
  target_link_libraries(judge-test gtest_main libtioj spdlog::spdlog)

  include(GoogleTest)
  gtest_discover_tests(judge-test)
endif()
