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
