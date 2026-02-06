# Main unit tests (Catch2)
add_executable(nexusfix_tests
    test_types.cpp
    test_parser.cpp
    test_memory.cpp
    test_market_data.cpp
    test_sbe.cpp
    test_queues.cpp
)

target_link_libraries(nexusfix_tests PRIVATE
    nexusfix
    Catch2::Catch2WithMain
)

if(MSVC)
    target_compile_options(nexusfix_tests PRIVATE /W4)
else()
    target_compile_options(nexusfix_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
include(Catch)
catch_discover_tests(nexusfix_tests)

# QuickFIX compatibility tests (standalone)
# TODO: Fix API mismatches in quickfix_compat.cpp (TICKET-011)
# - IndexedParser default constructor is private
# - FieldView API changed (has_value -> is_valid, member access)
# add_executable(quickfix_compat quickfix_compat.cpp)
# target_link_libraries(quickfix_compat PRIVATE nexusfix)
# add_test(NAME quickfix_compat COMMAND quickfix_compat)

# Platform abstraction tests (standalone, cross-platform)
add_executable(platform_test platform_test.cpp)
target_link_libraries(platform_test PRIVATE nexusfix)
if(MSVC)
    target_compile_options(platform_test PRIVATE /W4)
else()
    target_compile_options(platform_test PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(WIN32)
    target_link_libraries(platform_test PRIVATE ws2_32)
endif()
add_test(NAME platform_test COMMAND platform_test)

# Set output directory
set_target_properties(nexusfix_tests platform_test
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)
