cmake_minimum_required(VERSION 3.10)

project(tioj_judge VERSION 0.1 LANGUAGES CXX)
set(SOVERSION 0)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS_DEBUG "-Og -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# options
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
option(TIOJ_BUILD_TESTS "Build test programs" ON)

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
set(BUILD_TESTING OFF) # for tortellini

include(FetchContent)
FetchContent_Declare(
  cjail
  GIT_REPOSITORY "https://github.com/adrien1018/cjail.git"
  GIT_TAG "648b7c6c5f83ba5f0ac81988419f60e388e04d01"
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
  URL "https://github.com/nlohmann/json/archive/refs/tags/v3.10.5.zip"
)
FetchContent_Declare(
  ini
  GIT_REPOSITORY "https://github.com/Qix-/tortellini.git"
  GIT_TAG "0c9a2b79f6580d44bd7606f30668a5e0ba8326ae"
)
FetchContent_Declare(
  httplib
  URL "https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.10.8.zip"
)
FetchContent_Declare(
  sqlite_orm
  URL "https://github.com/fnc12/sqlite_orm/archive/refs/tags/v1.7.1.zip"
)
FetchContent_Declare(
  argparse
  URL "https://github.com/p-ranav/argparse/archive/refs/tags/v2.5.zip"
)

function (FetchContent_MakeAvailable_Exclude)
  foreach(dep IN LISTS ARGV)
    FetchContent_GetProperties(${dep})
    if(NOT ${dep}_POPULATED)
      FetchContent_Populate(${dep})
      add_subdirectory(${${dep}_SOURCE_DIR} ${${dep}_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()
  endforeach()
endfunction()
FetchContent_MakeAvailable_Exclude(cjail spdlog fmt json ini httplib sqlite_orm argparse)

# default scoring
add_executable(default-scoring "tools/default-scoring.cpp")
target_link_libraries(default-scoring nlohmann_json::nlohmann_json)
install(TARGETS default-scoring DESTINATION "${TIOJ_DATA_DIR}/")

# library
# required for any program that uses libtioj
set(TIOJ_DEPS nlohmann_json::nlohmann_json CJail::libcjail)
add_subdirectory(src)

# judge
file(GLOB JUDGE_SRC "src/*.cpp" "src/*.h")
add_executable(tioj-judge ${JUDGE_SRC})
target_link_libraries(tioj-judge libtioj ${TIOJ_DEPS} spdlog::spdlog fmt::fmt tortellini httplib::httplib sqlite_orm::sqlite_orm argparse::argparse)
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
  target_link_libraries(judge-test gtest_main libtioj ${TIOJ_DEPS} spdlog::spdlog)

  include(GoogleTest)
  gtest_discover_tests(judge-test)
endif()
