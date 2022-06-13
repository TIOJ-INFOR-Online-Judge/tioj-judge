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
add_subdirectory("${PROJECT_SOURCE_DIR}/deps/cjail" EXCLUDE_FROM_ALL)

add_executable(default-scoring "tools/default-scoring.cpp")
target_include_directories(default-scoring PRIVATE "deps/json/include")

file(GLOB JUDGE_SRC "src/*.cpp" "src/*.h")
add_executable(judge ${JUDGE_SRC})
target_include_directories(judge PRIVATE "deps/json/include" "deps/cjail/include")
target_link_libraries(judge libcjail)
