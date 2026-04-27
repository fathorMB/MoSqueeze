find_package(CLI11 CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_executable(mosqueeze
  src/main.cpp
  src/Terminal.cpp
  src/commands/CompressCommand.cpp
  src/commands/DecompressCommand.cpp
  src/commands/PredictCommand.cpp
)

target_link_libraries(mosqueeze
  PRIVATE
    libmosqueeze
    CLI11::CLI11
    fmt::fmt
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

if(WIN32)
  # Copy libmosqueeze.dll to CLI directory
  add_custom_command(TARGET mosqueeze POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:libmosqueeze>"
            "$<TARGET_FILE_DIR:mosqueeze>"
  )
  # Note: VCPKG_APPLOCAL_DEPS (set in root CMakeLists.txt) handles vcpkg DLL copying
endif()