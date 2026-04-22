find_package(CLI11 CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

if(NOT TARGET SQLite::SQLite3)
  add_library(SQLite::SQLite3 ALIAS unofficial::sqlite3::sqlite3)
endif()

add_executable(mosqueeze-bench
  src/main.cpp
  src/BenchmarkRunner.cpp
  src/CorpusManager.cpp
  src/Formatters.cpp
  src/ResultsStore.cpp
  src/ThreadPool.cpp
)

target_link_libraries(mosqueeze-bench
  PRIVATE
    libmosqueeze
    SQLite::SQLite3
    CLI11::CLI11
    fmt::fmt
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

target_include_directories(mosqueeze-bench
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(WIN32)
  if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.21")
    add_custom_command(TARGET mosqueeze-bench POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              $<TARGET_RUNTIME_DLLS:mosqueeze-bench>
              $<TARGET_FILE_DIR:mosqueeze-bench>
      COMMAND_EXPAND_LISTS
    )
  else()
    add_custom_command(TARGET mosqueeze-bench POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              $<TARGET_FILE:libmosqueeze>
              $<TARGET_FILE_DIR:mosqueeze-bench>
    )
  endif()
endif()
