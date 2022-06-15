cmake_minimum_required(VERSION 3.10)

project(tioj_judge VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

# configs
set(TIOJ_DATA_DIR "/var/lib/tioj-judge/" CACHE PATH "Path of default testdata & scoring program location")

add_compile_definitions(TIOJ_DATA_DIR="${TIOJ_DATA_DIR}")

# setup dependencies
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CJAIL_BUILD_TESTS OFF)

include(FetchContent)
FetchContent_Declare(
  cjail
  GIT_REPOSITORY "https://github.com/adrien1018/cjail.git"
  GIT_TAG "6c2147f2dfe475c3a8dc1c119490cb55cf77e5a7"
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
FetchContent_MakeAvailable(cjail spdlog fmt json ini)

# default scoring
add_executable(default-scoring "tools/default-scoring.cpp")
target_link_libraries(default-scoring nlohmann_json::nlohmann_json)
install(TARGETS default-scoring DESTINATION "${TIOJ_DATA_DIR}/")

# judge
file(GLOB JUDGE_SRC "src/*.cpp" "src/*.h")
set(JUDGE_LIBS nlohmann_json::nlohmann_json CJail::libcjail spdlog::spdlog fmt::fmt tortellini)
add_executable(judge ${JUDGE_SRC})
target_link_libraries(judge ${JUDGE_LIBS})
install(TARGETS judge DESTINATION bin)

# testing
option(TIOJ_BUILD_TESTS "Build test programs" ON)

if(TIOJ_BUILD_TESTS)
  enable_testing()
  FetchContent_Declare(
    googletest
    URL "https://github.com/google/googletest/archive/refs/tags/release-1.11.0.zip"
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  file(GLOB TEST_SRC "test/*.cpp" "test/*.h")
  add_executable(judge-test ${TEST_SRC} ${JUDGE_SRC})
  target_link_libraries(judge-test gtest_main ${JUDGE_LIBS})
  target_include_directories(judge-test PRIVATE ${DEPS_INCLUDE} "src")

  include(GoogleTest)
  gtest_discover_tests(judge-test)
endif()
