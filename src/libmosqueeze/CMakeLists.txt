find_package(zstd CONFIG REQUIRED)

add_library(libmosqueeze SHARED
  src/Version.cpp
  src/platform/Platform.cpp
  src/engines/ZstdEngine.cpp
  src/FileTypeDetector.cpp
)

set_target_properties(libmosqueeze PROPERTIES
  PREFIX ""
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)

target_include_directories(libmosqueeze
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(libmosqueeze
  PRIVATE
    zstd::libzstd
)

target_compile_features(libmosqueeze PUBLIC cxx_std_20)
