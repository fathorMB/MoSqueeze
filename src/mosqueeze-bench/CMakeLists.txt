add_executable(mosqueeze-bench
  src/main.cpp
)

target_link_libraries(mosqueeze-bench
  PRIVATE
    libmosqueeze
)

add_custom_command(TARGET mosqueeze-bench POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:libmosqueeze>
          $<TARGET_FILE_DIR:mosqueeze-bench>
)
