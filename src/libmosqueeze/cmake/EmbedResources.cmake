# EmbedResources.cmake
# Include this module from libmosqueeze's CMakeLists.txt to set up the
# compile-time embedding of combined_decision_matrix.json.
#
# After inclusion, the target "generate_decision_matrix_header" exists and
# ${DECISION_MATRIX_HEADER_OUTPUT} holds the path to the generated header.

set(DECISION_MATRIX_INPUT
    "${CMAKE_SOURCE_DIR}/docs/benchmarks/combined_decision_matrix.json")

set(DECISION_MATRIX_HEADER_OUTPUT
    "${CMAKE_CURRENT_BINARY_DIR}/decision_matrix_data.hpp")

add_custom_command(
    OUTPUT "${DECISION_MATRIX_HEADER_OUTPUT}"
    COMMAND ${CMAKE_COMMAND}
            -DINPUT=${DECISION_MATRIX_INPUT}
            -DOUTPUT=${DECISION_MATRIX_HEADER_OUTPUT}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedData.cmake
    DEPENDS "${DECISION_MATRIX_INPUT}"
    COMMENT "Embedding decision matrix JSON into decision_matrix_data.hpp..."
)

add_custom_target(generate_decision_matrix_header
    DEPENDS "${DECISION_MATRIX_HEADER_OUTPUT}"
)
