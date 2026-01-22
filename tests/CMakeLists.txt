find_package(Catch2 3 REQUIRED)

# Main unit tests (Catch2)
add_executable(nexusfix_tests
    test_types.cpp
    test_parser.cpp
    test_memory.cpp
    test_market_data.cpp
)

target_link_libraries(nexusfix_tests PRIVATE
    nexusfix
    Catch2::Catch2WithMain
)

target_compile_options(nexusfix_tests PRIVATE
    -Wall -Wextra -Wpedantic
)

include(CTest)
include(Catch)
catch_discover_tests(nexusfix_tests)

# QuickFIX compatibility tests (standalone)
add_executable(quickfix_compat quickfix_compat.cpp)
target_link_libraries(quickfix_compat PRIVATE nexusfix)

add_test(NAME quickfix_compat COMMAND quickfix_compat)

# Set output directory
set_target_properties(nexusfix_tests quickfix_compat
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)
