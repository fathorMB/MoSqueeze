# EmbedData.cmake
# Invoked as a CMake -P script:
#   cmake -DINPUT=<json_path> -DOUTPUT=<header_path> -P EmbedData.cmake
#
# Reads INPUT JSON and writes OUTPUT C++ header embedding only the
# "decision_matrix" object (strips the large "all_results" array).

cmake_policy(SET CMP0007 NEW)

if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "Usage: cmake -DINPUT=<json> -DOUTPUT=<header> -P EmbedData.cmake")
endif()

file(READ "${INPUT}" full_content)

# Extract the portion before "all_results" to avoid embedding benchmark records
string(FIND "${full_content}" "  \"all_results\"" cut_pos)
if(cut_pos GREATER 0)
    string(SUBSTRING "${full_content}" 0 ${cut_pos} head_content)
    # Remove trailing comma + whitespace, then close the JSON object
    string(REGEX REPLACE ",[ \t\n\r]*$" "" head_content "${head_content}")
    set(embedded_content "${head_content}\n}")
else()
    set(embedded_content "${full_content}")
endif()

file(WRITE "${OUTPUT}"
"// Auto-generated — do not edit
// Source: combined_decision_matrix.json (decision_matrix section)
#pragma once
#include <string_view>
namespace mosqueeze::resources {
inline constexpr std::string_view kDecisionMatrixJson = R\"json(
")
file(APPEND "${OUTPUT}" "${embedded_content}")
file(APPEND "${OUTPUT}"
"
)json\";
} // namespace mosqueeze::resources
")
