cmake_minimum_required(VERSION 3.10...3.26)

project(tioj_judge VERSION 0.1 LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOVERSION 0)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 20)
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
execute_process(
  COMMAND git submodule update --init --recursive --depth 1
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CJAIL_BUILD_CLI OFF)
set(CJAIL_BUILD_TESTS OFF)
set(HTTPLIB_REQUIRE_OPENSSL ON)
set(HTTPLIB_REQUIRE_ZLIB ON)
set(HTTPLIB_COMPILE ON)
set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF)
set(JSON_MultipleHeaders ON)
set(JSON_Install ON)
set(BUILD_TESTING OFF) # for tortellini

include(FetchContentExclude)
FetchContent_Declare(
  spdlog
  URL "https://github.com/gabime/spdlog/archive/refs/tags/v1.17.0.zip"
)

if(TIOJ_INSTALL_LIBTIOJ AND TIOJ_INSTALL_DEPENDENCIES)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libs/cjail)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libs/json)
else()
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libs/cjail EXCLUDE_FROM_ALL)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libs/json EXCLUDE_FROM_ALL)
endif()

find_package(OpenSSL REQUIRED)
find_package(zstd REQUIRED)
FetchContent_MakeAvailable_Exclude(spdlog)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libs/argparse EXCLUDE_FROM_ALL)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libs/cpp-httplib EXCLUDE_FROM_ALL)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libs/sqlite_orm EXCLUDE_FROM_ALL)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libs/tortellini EXCLUDE_FROM_ALL)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs/websocketpp)

find_library(JITTERENTROPY_LIB NAMES jitterentropy)

# Explicit Brotli linking is needed on Arch Linux
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" ".so")
find_library(BROTLI_ENC_LIBRARY NAMES brotlienc)
find_library(BROTLI_DEC_LIBRARY NAMES brotlidec)
if(BROTLI_ENC_LIBRARY)
  list(APPEND OPENSSL_LIBRARIES ${BROTLI_ENC_LIBRARY})
endif()
if(BROTLI_DEC_LIBRARY)
  list(APPEND OPENSSL_LIBRARIES ${BROTLI_DEC_LIBRARY})
endif()
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")

# detect Haskell version
find_program(HASKELL_EXECUTABLE ghc)
if(HASKELL_EXECUTABLE)
  execute_process(
    COMMAND ${HASKELL_EXECUTABLE} --numeric-version
    OUTPUT_VARIABLE HASKELL_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(HASKELL_VERSION VERSION_LESS "9.3")
    set(HASKELL_PROCESS_LIMIT 1)
  else()
    set(HASKELL_PROCESS_LIMIT 2)
  endif()
else()
  set(HASKELL_PROCESS_LIMIT 2)
endif()

# default scoring
add_executable(default-scoring "tools/default-scoring.cpp")
target_link_libraries(default-scoring nlohmann_json::nlohmann_json)
install(TARGETS default-scoring DESTINATION "${TIOJ_DATA_DIR}/")
# sandbox exec
# static link it to minimize memory usage; this will lead to some warnings about libc, but it is okay
add_executable(sandbox-exec "src/tioj/sandbox_main.cpp" "src/tioj/sandbox.cpp" "src/tioj/sandbox.h")
target_link_libraries(sandbox-exec CJail::libcjail)
target_compile_options(sandbox-exec PUBLIC -Os -static) # note that later flags will override previous ones
target_link_options(sandbox-exec PUBLIC -static -pthread -Wl,--gc-sections)
install(TARGETS sandbox-exec DESTINATION "${TIOJ_DATA_DIR}/")

# headers for judge
file(DOWNLOAD "https://raw.githubusercontent.com/nlohmann/json/v3.10.5/single_include/nlohmann/json.hpp"
  "${PROJECT_BINARY_DIR}/judge-headers/nlohmann/json.hpp")
file(DOWNLOAD "https://raw.githubusercontent.com/MikeMirzayanov/testlib/1e4e8a24c79c6bad3becbdb5a332ffc352b7d5dd/testlib.h"
  "${PROJECT_BINARY_DIR}/judge-headers/testlib.h")
file(DOWNLOAD "https://raw.githubusercontent.com/Kattis/problemtools/refs/tags/v1.20260620/examples/different/output_validators/different_validator/validate.h"
  "${PROJECT_BINARY_DIR}/judge-headers/validate.h")
execute_process(COMMAND
  patch "${PROJECT_BINARY_DIR}/judge-headers/testlib.h"
  -i "${CMAKE_CURRENT_SOURCE_DIR}/patches/tioj_testlib.patch"
  -o "${PROJECT_BINARY_DIR}/judge-headers/tioj_testlib.h")
install(DIRECTORY "${PROJECT_BINARY_DIR}/judge-headers" DESTINATION "${TIOJ_DATA_DIR}")

# library
# required for any program that uses libtioj
add_subdirectory(src)

# judge
file(GLOB JUDGE_SRC "src/*.cpp" "src/*.h")
add_executable(tioj-judge ${JUDGE_SRC})
target_compile_definitions(tioj-judge PRIVATE HASKELL_PROCESS_LIMIT=${HASKELL_PROCESS_LIMIT})
target_link_libraries(
    tioj-judge libtioj spdlog::spdlog tortellini
    httplib::httplib ${OPENSSL_LIBRARIES} zstd
    sqlite_orm::sqlite_orm argparse::argparse)
if(JITTERENTROPY_LIB)
  target_link_libraries(tioj-judge jitterentropy)
endif()
install(TARGETS tioj-judge DESTINATION bin)

# testing
if(TIOJ_BUILD_TESTS)
  enable_testing()
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libs/googletest EXCLUDE_FROM_ALL)

  file(GLOB TEST_SRC "test/*.cpp" "test/*.h")
  add_executable(judge-test ${TEST_SRC})
  target_compile_definitions(judge-test PRIVATE HASKELL_PROCESS_LIMIT=${HASKELL_PROCESS_LIMIT})
  target_link_libraries(judge-test gtest_main libtioj spdlog::spdlog)

  include(GoogleTest)
  gtest_discover_tests(judge-test DISCOVERY_MODE PRE_TEST)
endif()

# Formatting
file(GLOB_RECURSE ALL_SRC RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.cpp" "*.h")
list(FILTER ALL_SRC EXCLUDE REGEX "^libs/")
list(FILTER ALL_SRC EXCLUDE REGEX "^build/")
add_custom_target(format
  COMMAND clang-format -i ${ALL_SRC}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Formatting all source files"
)
