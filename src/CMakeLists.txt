file(GLOB LIBTIOJ_SRC "tioj/*.cpp" "tioj/*.h")
add_library(libtioj ${LIBTIOJ_SRC})
add_library(tioj::libtioj ALIAS libtioj)

set(TIOJ_LIBS ${TIOJ_DEPS} spdlog::spdlog fmt::fmt)

file(GLOB PUBLIC_HEADERS "${PROJECT_SOURCE_DIR}/include/tioj/*.h")

target_include_directories(libtioj
  PUBLIC
      $<INSTALL_INTERFACE:include/tioj>
      $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  PRIVATE
      ${PROJECT_SOURCE_DIR}/src/tioj
      ${PROJECT_BINARY_DIR}/include
)
target_link_libraries(libtioj PRIVATE ${TIOJ_LIBS})
set_target_properties(libtioj PROPERTIES
  OUTPUT_NAME "tioj"
  PREFIX "lib"
  POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS}
  VERSION ${PROJECT_VERSION} SOVERSION ${SOVERSION}
  PUBLIC_HEADER "${PUBLIC_HEADER_LIST}"
  LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib"
)

install(TARGETS libtioj
  EXPORT tioj_targets
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
