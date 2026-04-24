find_package(Catch2 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

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

# Use add_test (runs at CTest time, not build time) so all DLLs are in
# place before the binary executes. catch_discover_tests runs the binary
# at build time which triggers 0xc0000135 on Windows when vcpkg DLLs
# haven't been copied yet.
add_test(NAME prediction_tests COMMAND prediction_tests)
