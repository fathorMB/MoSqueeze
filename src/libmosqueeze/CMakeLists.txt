include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedResources.cmake)

find_package(zstd CONFIG REQUIRED)
find_package(LibLZMA REQUIRED)
find_package(unofficial-brotli CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
if(NOT TARGET SQLite::SQLite3 AND TARGET unofficial::sqlite3::sqlite3)
  add_library(SQLite::SQLite3 ALIAS unofficial::sqlite3::sqlite3)
endif()

add_library(libmosqueeze SHARED
  src/Version.cpp
  src/platform/Platform.cpp
  src/engines/ZstdEngine.cpp
  src/engines/LzmaEngine.cpp
  src/engines/BrotliEngine.cpp
  src/engines/ZpaqEngine.cpp
  src/FileTypeDetector.cpp
  src/RawFormat.cpp
  src/FileAnalyzer.cpp
  src/BenchmarkDatabase.cpp
  src/IntelligentSelector.cpp
  src/AlgorithmSelector.cpp
  src/PreprocessorSelector.cpp
  src/CompressionPipeline.cpp
  src/DecisionMatrix.cpp
  src/PredictionEngine.cpp
  src/preprocessors/JsonCanonicalizer.cpp
  src/preprocessors/XmlCanonicalizer.cpp
  src/preprocessors/ImageMetaStripper.cpp
  src/preprocessors/BayerPreprocessor.cpp
  src/preprocessors/RafParser.cpp
  src/preprocessors/DictionaryPreprocessor.cpp
  src/preprocessors/PngOptimizer.cpp
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
    ${CMAKE_CURRENT_BINARY_DIR}
)

add_dependencies(libmosqueeze generate_decision_matrix_header)

target_link_libraries(libmosqueeze
  PRIVATE
    zstd::libzstd
    LibLZMA::LibLZMA
    unofficial::brotli::brotlidec
    unofficial::brotli::brotlienc
    nlohmann_json::nlohmann_json
    pugixml::pugixml
    ZLIB::ZLIB
    SQLite::SQLite3
    libzpaq
)

target_compile_features(libmosqueeze PUBLIC cxx_std_20)
