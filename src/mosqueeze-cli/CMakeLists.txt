find_package(CLI11 CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

add_executable(mosqueeze
  src/main.cpp
)

target_link_libraries(mosqueeze
  PRIVATE
    libmosqueeze
    CLI11::CLI11
    fmt::fmt
    spdlog::spdlog
)

if(WIN32)
  if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.21")
    add_custom_command(TARGET mosqueeze POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              $<TARGET_RUNTIME_DLLS:mosqueeze>
              $<TARGET_FILE_DIR:mosqueeze>
      COMMAND_EXPAND_LISTS
    )
  else()
    add_custom_command(TARGET mosqueeze POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              $<TARGET_FILE:libmosqueeze>
              $<TARGET_FILE_DIR:mosqueeze>
    )
  endif()
endif()
