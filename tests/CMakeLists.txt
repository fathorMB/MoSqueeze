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
