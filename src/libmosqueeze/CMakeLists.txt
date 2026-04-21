add_library(libmosqueeze SHARED
  src/Version.cpp
  src/platform/Platform.cpp
)

set_target_properties(libmosqueeze PROPERTIES
  PREFIX ""
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)

target_include_directories(libmosqueeze
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(libmosqueeze PUBLIC cxx_std_20)
