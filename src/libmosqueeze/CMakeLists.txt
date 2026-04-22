find_package(zstd CONFIG REQUIRED)
find_package(LibLZMA REQUIRED)
find_package(unofficial-brotli CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)

add_library(libmosqueeze SHARED
  src/Version.cpp
  src/platform/Platform.cpp
  src/engines/ZstdEngine.cpp
  src/engines/LzmaEngine.cpp
  src/engines/BrotliEngine.cpp
  src/engines/ZpaqEngine.cpp
  src/FileTypeDetector.cpp
  src/AlgorithmSelector.cpp
  src/PreprocessorSelector.cpp
  src/CompressionPipeline.cpp
  src/preprocessors/JsonCanonicalizer.cpp
  src/preprocessors/XmlCanonicalizer.cpp
  src/preprocessors/ImageMetaStripper.cpp
  src/preprocessors/DictionaryPreprocessor.cpp
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
    LibLZMA::LibLZMA
    unofficial::brotli::brotlidec
    unofficial::brotli::brotlienc
    nlohmann_json::nlohmann_json
    pugixml::pugixml
    libzpaq
)

target_compile_features(libmosqueeze PUBLIC cxx_std_20)
