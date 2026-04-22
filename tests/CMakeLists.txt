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
