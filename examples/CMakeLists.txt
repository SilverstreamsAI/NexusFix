# examples/CMakeLists.txt
# NexusFIX Examples

# Simple FIX client example
add_executable(simple_client simple_client.cpp)
target_link_libraries(simple_client PRIVATE nexusfix)

# Set output directory
set_target_properties(simple_client
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/examples
)
