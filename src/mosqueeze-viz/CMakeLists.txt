find_package(CLI11 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)

if(NOT TARGET SQLite::SQLite3)
  add_library(SQLite::SQLite3 ALIAS unofficial::sqlite3::sqlite3)
endif()

add_library(mosqueeze-viz-lib
  src/DataLoader.cpp
  src/ComparisonEngine.cpp
  src/DashboardGenerator.cpp
  src/MarkdownExporter.cpp
  src/Charts.cpp
)

target_include_directories(mosqueeze-viz-lib
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(mosqueeze-viz-lib
  PUBLIC
    nlohmann_json::nlohmann_json
    SQLite::SQLite3
)

add_executable(mosqueeze-viz
  src/main.cpp
)

target_link_libraries(mosqueeze-viz
  PRIVATE
    mosqueeze-viz-lib
    CLI11::CLI11
)

if(WIN32)
  add_custom_command(TARGET mosqueeze-viz POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:mosqueeze-viz-lib>"
            "$<TARGET_FILE_DIR:mosqueeze-viz>"
  )
endif()
