add_executable(Version_test
  unit/Version_test.cpp
)

target_link_libraries(Version_test
  PRIVATE
    libmosqueeze
)

add_custom_command(TARGET Version_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:libmosqueeze>
          $<TARGET_FILE_DIR:Version_test>
)

add_test(NAME Version_test COMMAND Version_test)

add_executable(ZstdEngine_test
  unit/ZstdEngine_test.cpp
)

target_link_libraries(ZstdEngine_test
  PRIVATE
    libmosqueeze
)

add_custom_command(TARGET ZstdEngine_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:libmosqueeze>
          $<TARGET_FILE_DIR:ZstdEngine_test>
)

add_test(NAME ZstdEngine_test COMMAND ZstdEngine_test)

add_executable(LzmaEngine_test
  unit/LzmaEngine_test.cpp
)

target_link_libraries(LzmaEngine_test
  PRIVATE
    libmosqueeze
)

add_custom_command(TARGET LzmaEngine_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:libmosqueeze>
          $<TARGET_FILE_DIR:LzmaEngine_test>
)

add_test(NAME LzmaEngine_test COMMAND LzmaEngine_test)

add_executable(BrotliEngine_test
  unit/BrotliEngine_test.cpp
)

target_link_libraries(BrotliEngine_test
  PRIVATE
    libmosqueeze
)

add_custom_command(TARGET BrotliEngine_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:libmosqueeze>
          $<TARGET_FILE_DIR:BrotliEngine_test>
)

add_test(NAME BrotliEngine_test COMMAND BrotliEngine_test)

add_executable(ZpaqEngine_test
  unit/ZpaqEngine_test.cpp
)

target_link_libraries(ZpaqEngine_test
  PRIVATE
    libmosqueeze
)

add_custom_command(TARGET ZpaqEngine_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:libmosqueeze>
          $<TARGET_FILE_DIR:ZpaqEngine_test>
)

add_test(NAME ZpaqEngine_test COMMAND ZpaqEngine_test)

add_executable(VideoFileType_test
  unit/VideoFileType_test.cpp
)

target_link_libraries(VideoFileType_test
  PRIVATE
    libmosqueeze
)

add_custom_command(TARGET VideoFileType_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:libmosqueeze>
          $<TARGET_FILE_DIR:VideoFileType_test>
)

add_test(NAME VideoFileType_test COMMAND VideoFileType_test)

add_executable(FileTypeDetector_test
  unit/FileTypeDetector_test.cpp
)

target_link_libraries(FileTypeDetector_test
  PRIVATE
    libmosqueeze
)

add_custom_command(TARGET FileTypeDetector_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:libmosqueeze>
          $<TARGET_FILE_DIR:FileTypeDetector_test>
)

add_test(NAME FileTypeDetector_test COMMAND FileTypeDetector_test)

add_executable(FileAnalyzer_test
  unit/FileAnalyzer_test.cpp
)
target_link_libraries(FileAnalyzer_test PRIVATE libmosqueeze)
add_custom_command(TARGET FileAnalyzer_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:libmosqueeze>
          $<TARGET_FILE_DIR:FileAnalyzer_test>
)
add_test(NAME FileAnalyzer_test COMMAND FileAnalyzer_test)

add_executable(BenchmarkDatabase_test
  unit/BenchmarkDatabase_test.cpp
)
target_link_libraries(BenchmarkDatabase_test PRIVATE libmosqueeze)
add_custom_command(TARGET BenchmarkDatabase_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:libmosqueeze>
          $<TARGET_FILE_DIR:BenchmarkDatabase_test>
)
add_test(NAME BenchmarkDatabase_test COMMAND BenchmarkDatabase_test)

add_executable(IntelligentSelector_test
  unit/IntelligentSelector_test.cpp
)
target_link_libraries(IntelligentSelector_test PRIVATE libmosqueeze)
add_custom_command(TARGET IntelligentSelector_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:libmosqueeze>
          $<TARGET_FILE_DIR:IntelligentSelector_test>
)
add_test(NAME IntelligentSelector_test COMMAND IntelligentSelector_test)

add_executable(RawFormat_test
  unit/RawFormat_test.cpp
)
target_link_libraries(RawFormat_test
  PRIVATE
    libmosqueeze
)
add_custom_command(TARGET RawFormat_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:libmosqueeze>
          $<TARGET_FILE_DIR:RawFormat_test>
)
add_test(NAME RawFormat_test COMMAND RawFormat_test)

add_executable(AlgorithmSelector_test
  unit/AlgorithmSelector_test.cpp
)

target_link_libraries(AlgorithmSelector_test
  PRIVATE
    libmosqueeze
)

add_custom_command(TARGET AlgorithmSelector_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:libmosqueeze>
          $<TARGET_FILE_DIR:AlgorithmSelector_test>
)

add_test(NAME AlgorithmSelector_test COMMAND AlgorithmSelector_test)

add_executable(PreprocessorSelector_test
  unit/PreprocessorSelector_test.cpp
)
target_link_libraries(PreprocessorSelector_test
  PRIVATE
    libmosqueeze
)
add_custom_command(TARGET PreprocessorSelector_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:libmosqueeze>
          $<TARGET_FILE_DIR:PreprocessorSelector_test>
)
add_test(NAME PreprocessorSelector_test COMMAND PreprocessorSelector_test)

add_executable(BenchmarkConfig_test
  unit/BenchmarkConfig_test.cpp
)

target_link_libraries(BenchmarkConfig_test
  PRIVATE
    libmosqueeze
)

add_custom_command(TARGET BenchmarkConfig_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:libmosqueeze>
          $<TARGET_FILE_DIR:BenchmarkConfig_test>
)

add_test(NAME BenchmarkConfig_test COMMAND BenchmarkConfig_test)

add_executable(BenchmarkPreprocess_test
  unit/BenchmarkPreprocess_test.cpp
  ../src/mosqueeze-bench/src/BenchmarkRunner.cpp
  ../src/mosqueeze-bench/src/ResultsStore.cpp
  ../src/mosqueeze-bench/src/ThreadPool.cpp
)

target_include_directories(BenchmarkPreprocess_test
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src/mosqueeze-bench/include
)

find_package(unofficial-sqlite3 CONFIG REQUIRED)
if(NOT TARGET SQLite::SQLite3 AND TARGET unofficial::sqlite3::sqlite3)
  add_library(SQLite::SQLite3 ALIAS unofficial::sqlite3::sqlite3)
endif()

target_link_libraries(BenchmarkPreprocess_test
  PRIVATE
    libmosqueeze
    SQLite::SQLite3
)

add_custom_command(TARGET BenchmarkPreprocess_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:libmosqueeze>
          $<TARGET_FILE_DIR:BenchmarkPreprocess_test>
)

add_test(NAME BenchmarkPreprocess_test COMMAND BenchmarkPreprocess_test)

add_executable(BenchmarkExtendedMatrix_test
  unit/BenchmarkExtendedMatrix_test.cpp
  ../src/mosqueeze-bench/src/BenchmarkRunner.cpp
  ../src/mosqueeze-bench/src/ResultsStore.cpp
  ../src/mosqueeze-bench/src/ThreadPool.cpp
)
target_include_directories(BenchmarkExtendedMatrix_test
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src/mosqueeze-bench/include
)
target_link_libraries(BenchmarkExtendedMatrix_test
  PRIVATE
    libmosqueeze
    SQLite::SQLite3
)
add_custom_command(TARGET BenchmarkExtendedMatrix_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:libmosqueeze>
          $<TARGET_FILE_DIR:BenchmarkExtendedMatrix_test>
)
add_test(NAME BenchmarkExtendedMatrix_test COMMAND BenchmarkExtendedMatrix_test)

add_executable(ProgressReporter_test
  unit/ProgressReporter_test.cpp
  ../src/mosqueeze-bench/src/ProgressReporter.cpp
)

target_include_directories(ProgressReporter_test
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src/mosqueeze-bench/include
    ${CMAKE_SOURCE_DIR}/src/libmosqueeze/include
)

add_test(NAME ProgressReporter_test COMMAND ProgressReporter_test)

add_executable(ThreadPool_test
  unit/ThreadPool_test.cpp
  ../src/mosqueeze-bench/src/ThreadPool.cpp
)

target_include_directories(ThreadPool_test
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src/mosqueeze-bench/include
)

add_test(NAME ThreadPool_test COMMAND ThreadPool_test)

add_executable(JsonCanonicalizer_test
  unit/JsonCanonicalizer_test.cpp
)
target_link_libraries(JsonCanonicalizer_test PRIVATE libmosqueeze)
add_custom_command(TARGET JsonCanonicalizer_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:libmosqueeze>
          $<TARGET_FILE_DIR:JsonCanonicalizer_test>
)
add_test(NAME JsonCanonicalizer_test COMMAND JsonCanonicalizer_test)

add_executable(XmlCanonicalizer_test
  unit/XmlCanonicalizer_test.cpp
)
target_link_libraries(XmlCanonicalizer_test PRIVATE libmosqueeze)
add_custom_command(TARGET XmlCanonicalizer_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:libmosqueeze>
          $<TARGET_FILE_DIR:XmlCanonicalizer_test>
)
add_test(NAME XmlCanonicalizer_test COMMAND XmlCanonicalizer_test)

add_executable(ImageMetaStripper_test
  unit/ImageMetaStripper_test.cpp
)
target_link_libraries(ImageMetaStripper_test PRIVATE libmosqueeze)
add_custom_command(TARGET ImageMetaStripper_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:libmosqueeze>
          $<TARGET_FILE_DIR:ImageMetaStripper_test>
)
add_test(NAME ImageMetaStripper_test COMMAND ImageMetaStripper_test)

add_executable(PngOptimizer_test
  unit/PngOptimizer_test.cpp
)
target_link_libraries(PngOptimizer_test PRIVATE libmosqueeze)
add_custom_command(TARGET PngOptimizer_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:libmosqueeze>
          $<TARGET_FILE_DIR:PngOptimizer_test>
)
add_test(NAME PngOptimizer_test COMMAND PngOptimizer_test)

add_executable(BayerPreprocessor_test
  unit/BayerPreprocessor_test.cpp
)
target_link_libraries(BayerPreprocessor_test PRIVATE libmosqueeze)
add_custom_command(TARGET BayerPreprocessor_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:libmosqueeze>
          $<TARGET_FILE_DIR:BayerPreprocessor_test>
)
add_test(NAME BayerPreprocessor_test COMMAND BayerPreprocessor_test)

add_executable(RafParser_test
  unit/RafParser_test.cpp
)
target_include_directories(RafParser_test
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src/libmosqueeze/src
)
target_link_libraries(RafParser_test PRIVATE libmosqueeze)
add_custom_command(TARGET RafParser_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:libmosqueeze>
          $<TARGET_FILE_DIR:RafParser_test>
)
add_test(NAME RafParser_test COMMAND RafParser_test)

add_executable(DictionaryPreprocessor_test
  unit/DictionaryPreprocessor_test.cpp
)
target_link_libraries(DictionaryPreprocessor_test PRIVATE libmosqueeze)
add_custom_command(TARGET DictionaryPreprocessor_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:libmosqueeze>
          $<TARGET_FILE_DIR:DictionaryPreprocessor_test>
)
add_test(NAME DictionaryPreprocessor_test COMMAND DictionaryPreprocessor_test)

add_executable(Pipeline_test
  unit/Pipeline_test.cpp
)
target_link_libraries(Pipeline_test PRIVATE libmosqueeze)
add_custom_command(TARGET Pipeline_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:libmosqueeze>
          $<TARGET_FILE_DIR:Pipeline_test>
)
add_test(NAME Pipeline_test COMMAND Pipeline_test)

add_executable(PredictionEngine_test
  unit/PredictionEngine_test.cpp
)
target_link_libraries(PredictionEngine_test PRIVATE libmosqueeze)
target_compile_definitions(PredictionEngine_test PRIVATE
    DECISION_MATRIX_JSON_PATH="${CMAKE_SOURCE_DIR}/docs/benchmarks/combined_decision_matrix.json"
)
add_custom_command(TARGET PredictionEngine_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:libmosqueeze>
          $<TARGET_FILE_DIR:PredictionEngine_test>
)
add_test(NAME PredictionEngine_test COMMAND PredictionEngine_test)

add_executable(DecisionMatrix_test
  unit/DecisionMatrix_test.cpp
)
target_link_libraries(DecisionMatrix_test PRIVATE libmosqueeze)
target_compile_definitions(DecisionMatrix_test PRIVATE
    DECISION_MATRIX_JSON_PATH="${CMAKE_SOURCE_DIR}/docs/benchmarks/combined_decision_matrix.json"
)
add_custom_command(TARGET DecisionMatrix_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:libmosqueeze>
          $<TARGET_FILE_DIR:DecisionMatrix_test>
)
add_test(NAME DecisionMatrix_test COMMAND DecisionMatrix_test)

add_executable(Viz_test
  unit/Viz_test.cpp
)
target_link_libraries(Viz_test PRIVATE mosqueeze-viz-lib)
add_custom_command(TARGET Viz_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:mosqueeze-viz-lib>
          $<TARGET_FILE_DIR:Viz_test>
)
add_test(NAME Viz_test COMMAND Viz_test)

add_subdirectory(prediction)
