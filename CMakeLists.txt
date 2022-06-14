cmake_minimum_required(VERSION 3.10)

project(tioj_judge VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

# build submodule dependencies
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
  message(STATUS "Submodule update")
  execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                  RESULT_VARIABLE GIT_SUBMOD_RESULT)
  if(NOT GIT_SUBMOD_RESULT EQUAL "0")
    message(FATAL_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
  endif()
endif()
if(NOT EXISTS "${PROJECT_SOURCE_DIR}/deps/cjail/CMakeLists.txt")
  message(FATAL_ERROR "The submodules were not downloaded! Please update submodules and try again.")
endif()

# submodules
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CJAIL_BUILD_TESTS OFF)
add_subdirectory("${PROJECT_SOURCE_DIR}/deps/cjail" EXCLUDE_FROM_ALL)
add_subdirectory("${PROJECT_SOURCE_DIR}/deps/spdlog" EXCLUDE_FROM_ALL)
add_subdirectory("${PROJECT_SOURCE_DIR}/deps/fmt" EXCLUDE_FROM_ALL)

# default scoring
add_executable(default-scoring "tools/default-scoring.cpp")
target_include_directories(default-scoring PRIVATE "deps/json/include")

# judge
file(GLOB JUDGE_SRC "src/*.cpp" "src/*.h")
file(GLOB DEPS_INCLUDE "deps/*/include")
set(JUDGE_LIBS libcjail spdlog::spdlog fmt::fmt)
add_executable(judge ${JUDGE_SRC})
target_include_directories(judge PRIVATE ${DEPS_INCLUDE})
target_link_libraries(judge ${JUDGE_LIBS})

# testing
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

file(GLOB TEST_SRC "test/*.cpp" "test/*.h")
add_executable(judge-test ${TEST_SRC} ${JUDGE_SRC})
target_link_libraries(judge-test gtest_main ${JUDGE_LIBS})
target_include_directories(judge-test PRIVATE ${DEPS_INCLUDE} "src")

include(GoogleTest)
gtest_discover_tests(judge-test)
