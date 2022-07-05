file(GLOB LIBTIOJ_SRC "tioj/*.cpp" "tioj/*.h")
add_library(libtioj ${LIBTIOJ_SRC})
add_library(tioj::libtioj ALIAS libtioj)

set(TIOJ_DEPS nlohmann_json::nlohmann_json CJail::libcjail)

file(GLOB PUBLIC_HEADERS "${PROJECT_SOURCE_DIR}/include/tioj/*.h")

target_include_directories(libtioj
  PUBLIC
      $<INSTALL_INTERFACE:include/tioj>
      $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  PRIVATE
      ${PROJECT_SOURCE_DIR}/src/tioj
      ${PROJECT_BINARY_DIR}/include
)
target_link_libraries(libtioj PUBLIC ${TIOJ_DEPS} PRIVATE spdlog::spdlog fmt::fmt libseccomp::libseccomp)
set_target_properties(libtioj PROPERTIES
  OUTPUT_NAME "tioj-only"
  PREFIX "lib"
  POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS}
  VERSION ${PROJECT_VERSION} SOVERSION ${SOVERSION}
  PUBLIC_HEADER "${PUBLIC_HEADERS}"
  LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib"
)

get_target_property(test_var CJail::libcjail INTERFACE_INCLUDE_DIRECTORIES)

list(APPEND CMAKE_MODULE_PATH "${cjail_SOURCE_DIR}/cmake")
find_package(libseccomp REQUIRED)
find_package(Libnl REQUIRED
    COMPONENTS
        libnl
        genl
)
include(BundleStaticLibrary)
bundle_static_library(libtioj tioj-standalone "bsd")

if(TIOJ_INSTALL_LIBTIOJ)
  install(TARGETS libtioj
    EXPORT tioj_targets
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" EXCLUDE_FROM_ALL
    PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/tioj"
  )
  install(FILES "${CMAKE_BINARY_DIR}/libtioj-standalone.a"
    DESTINATION ${CMAKE_INSTALL_LIBDIR} RENAME libtioj.a
  )
endif()
