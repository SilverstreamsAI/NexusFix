# benchmarks/CMakeLists.txt
# NexusFIX Benchmarks

# Parse latency benchmark
add_executable(parse_benchmark parse_benchmark.cpp)
target_link_libraries(parse_benchmark PRIVATE nexusfix)

# Session throughput benchmark
add_executable(session_benchmark session_benchmark.cpp)
target_link_libraries(session_benchmark PRIVATE nexusfix)

# Set output directory
set_target_properties(parse_benchmark session_benchmark
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/benchmarks
)

# Optimization flags for benchmarks
target_compile_options(parse_benchmark PRIVATE -O3 -march=native)
target_compile_options(session_benchmark PRIVATE -O3 -march=native)

# QuickFIX comparison benchmark (optional)
add_subdirectory(vs_quickfix)
