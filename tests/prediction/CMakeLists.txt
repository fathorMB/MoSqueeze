find_package(Catch2 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
include(Catch)

add_executable(prediction_tests
    DecisionMatrixTest.cpp
    PredictionEngineTest.cpp
    PredictCommandIntegrationTest.cpp
)

target_link_libraries(prediction_tests
  PRIVATE
    libmosqueeze
    Catch2::Catch2WithMain
    nlohmann_json::nlohmann_json
)

target_compile_definitions(prediction_tests PRIVATE
    DECISION_MATRIX_JSON_PATH="${CMAKE_SOURCE_DIR}/docs/benchmarks/combined_decision_matrix.json"
    MOSQUEEZE_CLI_PATH="$<TARGET_FILE:mosqueeze>"
)

# Copy libmosqueeze DLL next to the test binary on Windows
add_custom_command(TARGET prediction_tests POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:libmosqueeze>
          $<TARGET_FILE_DIR:prediction_tests>
)

catch_discover_tests(prediction_tests)
