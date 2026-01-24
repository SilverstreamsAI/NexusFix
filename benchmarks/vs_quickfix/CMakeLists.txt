# vs_quickfix/CMakeLists.txt
# QuickFIX Comparison Benchmark

# Find QuickFIX library
find_library(QUICKFIX_LIBRARY quickfix)
find_path(QUICKFIX_INCLUDE_DIR quickfix/Message.h)

if(QUICKFIX_LIBRARY AND QUICKFIX_INCLUDE_DIR)
    message(STATUS "QuickFIX found: ${QUICKFIX_LIBRARY}")
    message(STATUS "QuickFIX include: ${QUICKFIX_INCLUDE_DIR}")

    # QuickFIX-only benchmark (C++14 for compatibility with old QuickFIX headers)
    add_executable(quickfix_benchmark
        quickfix_only_benchmark.cpp
    )

    target_include_directories(quickfix_benchmark PRIVATE
        ${QUICKFIX_INCLUDE_DIR}
    )

    target_link_libraries(quickfix_benchmark PRIVATE
        ${QUICKFIX_LIBRARY}
        pthread
    )

    # Force C++14 for QuickFIX compatibility (old exception specs not allowed in C++17+)
    set_target_properties(quickfix_benchmark PROPERTIES
        CXX_STANDARD 14
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/benchmarks"
    )

    target_compile_options(quickfix_benchmark PRIVATE -O3 -march=native)

    message(STATUS "QuickFIX benchmark enabled (C++14 mode)")
else()
    message(WARNING "QuickFIX not found - comparison benchmark disabled")
    message(WARNING "Install with: sudo apt-get install libquickfix-dev")
endif()
