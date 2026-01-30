# benchmarks/CMakeLists.txt
# NexusFIX Benchmarks

# Parse latency benchmark
add_executable(parse_benchmark parse_benchmark.cpp)
target_link_libraries(parse_benchmark PRIVATE nexusfix)

# Session throughput benchmark
add_executable(session_benchmark session_benchmark.cpp)
target_link_libraries(session_benchmark PRIVATE nexusfix)

# Timing comparison benchmark (rdtscp vs nanobench)
add_executable(timing_comparison timing_comparison.cpp)
target_link_libraries(timing_comparison PRIVATE nexusfix)
target_include_directories(timing_comparison PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Set output directory
set_target_properties(parse_benchmark session_benchmark timing_comparison
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/benchmarks
)

# Hash map benchmark (std::unordered_map vs absl::flat_hash_map)
if(NFX_ENABLE_ABSEIL)
    add_executable(hash_map_bench hash_map_bench.cpp)
    target_link_libraries(hash_map_bench PRIVATE nexusfix)
    # Disable strict warnings for Abseil compatibility (uses __int128)
    target_compile_options(hash_map_bench PRIVATE
        -O3 -march=native
        -Wno-pedantic -Wno-overflow -Wno-error
    )
    set_target_properties(hash_map_bench PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/benchmarks
    )
endif()

# CPU affinity benchmark
add_executable(cpu_affinity_bench cpu_affinity_bench.cpp)
target_link_libraries(cpu_affinity_bench PRIVATE nexusfix pthread)
target_compile_options(cpu_affinity_bench PRIVATE -O3 -march=native)
set_target_properties(cpu_affinity_bench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/benchmarks
)

# Deferred processor benchmark
add_executable(deferred_processor_bench deferred_processor_bench.cpp)
target_link_libraries(deferred_processor_bench PRIVATE nexusfix pthread)
target_compile_options(deferred_processor_bench PRIVATE -O3 -march=native -Wno-interference-size)
set_target_properties(deferred_processor_bench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/benchmarks
)

# Optimization summary benchmark (before vs after all optimizations)
add_executable(optimization_summary_bench optimization_summary_bench.cpp)
target_link_libraries(optimization_summary_bench PRIVATE nexusfix pthread)
target_compile_options(optimization_summary_bench PRIVATE -O3 -march=native -Wno-interference-size -Wno-pedantic -Wno-overflow -Wno-error)
set_target_properties(optimization_summary_bench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/benchmarks
)

# Registered buffers benchmark (io_uring)
add_executable(registered_buffers_bench registered_buffers_bench.cpp)
target_link_libraries(registered_buffers_bench PRIVATE nexusfix pthread uring)
target_compile_options(registered_buffers_bench PRIVATE -O3 -march=native)
set_target_properties(registered_buffers_bench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/benchmarks
)

# Multishot receive benchmark (io_uring)
add_executable(multishot_recv_bench multishot_recv_bench.cpp)
target_link_libraries(multishot_recv_bench PRIVATE nexusfix pthread uring)
target_compile_options(multishot_recv_bench PRIVATE -O3 -march=native)
set_target_properties(multishot_recv_bench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/benchmarks
)

# Thread-local pool benchmark
add_executable(thread_local_pool_bench thread_local_pool_bench.cpp)
target_link_libraries(thread_local_pool_bench PRIVATE nexusfix pthread)
target_compile_options(thread_local_pool_bench PRIVATE -O3 -march=native -Wno-interference-size)
set_target_properties(thread_local_pool_bench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/benchmarks
)

# Message store PMR benchmark (before vs after PMR optimization)
add_executable(message_store_pmr_bench message_store_pmr_bench.cpp)
target_link_libraries(message_store_pmr_bench PRIVATE nexusfix pthread)
target_compile_options(message_store_pmr_bench PRIVATE -O3 -march=native)
set_target_properties(message_store_pmr_bench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/benchmarks
)

# Compile-time serializer benchmark
add_executable(constexpr_serializer_bench constexpr_serializer_bench.cpp)
target_link_libraries(constexpr_serializer_bench PRIVATE nexusfix pthread)
target_compile_options(constexpr_serializer_bench PRIVATE -O3 -march=native)
set_target_properties(constexpr_serializer_bench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/benchmarks
)

# SIMD checksum benchmark
add_executable(simd_checksum_bench simd_checksum_bench.cpp)
target_link_libraries(simd_checksum_bench PRIVATE nexusfix pthread)
target_compile_options(simd_checksum_bench PRIVATE -O3 -march=native)
set_target_properties(simd_checksum_bench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/benchmarks
)

# Batch submit benchmark (io_uring)
add_executable(batch_submit_bench batch_submit_bench.cpp)
target_link_libraries(batch_submit_bench PRIVATE nexusfix pthread uring)
target_compile_options(batch_submit_bench PRIVATE -O3 -march=native)
set_target_properties(batch_submit_bench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/benchmarks
)

# io_uring transport integration benchmark (registered buffers + multishot)
add_executable(io_uring_transport_integration_bench io_uring_transport_integration_bench.cpp)
target_link_libraries(io_uring_transport_integration_bench PRIVATE nexusfix pthread uring)
target_compile_options(io_uring_transport_integration_bench PRIVATE -O3 -march=native)
set_target_properties(io_uring_transport_integration_bench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/benchmarks
)

# MPSC queue benchmark (Disruptor-pattern multi-producer single-consumer)
add_executable(mpsc_queue_bench mpsc_queue_bench.cpp)
target_link_libraries(mpsc_queue_bench PRIVATE nexusfix pthread)
target_compile_options(mpsc_queue_bench PRIVATE -O3 -march=native -Wno-interference-size)
set_target_properties(mpsc_queue_bench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/benchmarks
)

# Optimization flags for benchmarks
target_compile_options(parse_benchmark PRIVATE -O3 -march=native)
target_compile_options(session_benchmark PRIVATE -O3 -march=native)
target_compile_options(timing_comparison PRIVATE -O3 -march=native)

# QuickFIX comparison benchmark (optional)
add_subdirectory(vs_quickfix)
