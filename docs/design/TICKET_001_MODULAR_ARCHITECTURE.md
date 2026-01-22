# TICKET_001: NexusFIX Modular Architecture

**Status**: APPROVED
**Category**: Architecture / Design Pattern

## 1. Overview

NexusFIX adopts a strict modular architecture with clear separation between **Interface Modules** and **Implementation Modules**.

## 2. Architecture Principles

### 2.1 Interface Modules (Headers)

- **Pure abstractions** - Concepts, type traits, base classes
- **Zero implementation** - No function bodies in interface headers
- **Stable API** - Interface changes require version bump
- **Documentation** - Each interface fully documented

### 2.2 Implementation Modules

- **Single responsibility** - One module, one purpose
- **Pluggable** - Implementations can be swapped without recompilation
- **Testable** - Each module independently testable
- **No cross-dependencies** - Modules communicate via interfaces only

## 3. Module Structure

```
include/nexusfix/
├── interfaces/                 # Interface Modules (pure abstractions)
│   ├── i_parser.hpp           # Parser interface (concept)
│   ├── i_session.hpp          # Session interface
│   ├── i_transport.hpp        # Transport interface
│   ├── i_message.hpp          # Message interface
│   └── i_allocator.hpp        # Allocator interface
│
├── messages/                   # Message Type Definitions
│   ├── fix44/                 # FIX 4.4 message types
│   │   ├── new_order_single.hpp
│   │   ├── execution_report.hpp
│   │   └── order_cancel.hpp
│   └── common/                # Shared message components
│       ├── header.hpp
│       └── trailer.hpp
│
├── types/                      # Strong Types & Constants
│   ├── field_types.hpp        # Price, Volume, Quantity
│   ├── tag_types.hpp          # FIX tag definitions
│   └── error_types.hpp        # Error codes
│
└── utils/                      # Utilities (header-only)
    ├── benchmark_utils.hpp
    └── memory_utils.hpp

src/
├── parser/                     # Implementation Modules
│   ├── consteval_parser.cpp   # Compile-time offset parser
│   └── simd_parser.cpp        # SIMD-accelerated parser
│
├── session/
│   ├── session_manager.cpp    # Session lifecycle
│   └── heartbeat.cpp          # Heartbeat logic
│
├── transport/
│   ├── tcp_transport.cpp      # Standard TCP
│   └── io_uring_transport.cpp # io_uring (Linux)
│
└── memory/
    ├── pmr_pool.cpp           # PMR memory pool
    └── huge_page_alloc.cpp    # Huge page allocator
```

## 4. Interface Design Pattern

### 4.1 Concept-based Interfaces (Preferred)

```cpp
// interfaces/i_parser.hpp
namespace nfx {

template <typename T>
concept Parser = requires(T parser, std::span<const std::byte> data) {
    { parser.parse(data) } -> std::same_as<std::expected<Message, ParseError>>;
    { parser.supports_message_type(MsgType{}) } -> std::same_as<bool>;
    { T::name() } -> std::convertible_to<std::string_view>;
};

} // namespace nfx
```

### 4.2 Implementation Registration

```cpp
// src/parser/consteval_parser.cpp
namespace nfx::impl {

class ConstevalParser {
public:
    static constexpr std::string_view name() { return "consteval_parser"; }

    [[nodiscard]] std::expected<Message, ParseError>
    parse(std::span<const std::byte> data) noexcept;

    [[nodiscard]] bool
    supports_message_type(MsgType type) const noexcept;
};

static_assert(Parser<ConstevalParser>);  // Compile-time verification

} // namespace nfx::impl
```

## 5. Module Dependency Rules

### 5.1 Allowed Dependencies

```
interfaces/ → (none)           # Interfaces depend on nothing
types/      → (none)           # Types depend on nothing
utils/      → types/           # Utils may use types
messages/   → types/, interfaces/
src/*       → interfaces/, types/, utils/, messages/
tests/      → (all)
benchmarks/ → (all)
```

### 5.2 Prohibited Dependencies

- Implementation modules MUST NOT depend on each other directly
- Interface modules MUST NOT include implementation headers
- No circular dependencies allowed

## 6. Module Boundaries

| Module | Responsibility | Hot Path |
|--------|----------------|----------|
| `parser/` | FIX message parsing | YES |
| `session/` | Session state machine | NO |
| `transport/` | Network I/O | YES |
| `memory/` | Memory allocation | YES |
| `messages/` | Message type definitions | YES |

## 7. Testing Strategy

### 7.1 Unit Tests (Per Module)

```
tests/
├── unit/
│   ├── parser_test.cpp        # Test parser implementations
│   ├── session_test.cpp       # Test session logic
│   └── transport_test.cpp     # Test transport layer
```

### 7.2 Interface Compliance Tests

```cpp
// tests/compliance/parser_compliance_test.cpp
template <Parser P>
void test_parser_compliance() {
    P parser;
    // Test all interface requirements
}

// Run for each implementation
TEST(ParserCompliance, ConstevalParser) {
    test_parser_compliance<ConstevalParser>();
}

TEST(ParserCompliance, SimdParser) {
    test_parser_compliance<SimdParser>();
}
```

## 8. Build Configuration

### 8.1 CMake Module Structure

```cmake
# Each module is a separate library
add_library(nfx_parser STATIC
    src/parser/consteval_parser.cpp
    src/parser/simd_parser.cpp
)

add_library(nfx_session STATIC
    src/session/session_manager.cpp
    src/session/heartbeat.cpp
)

add_library(nfx_transport STATIC
    src/transport/tcp_transport.cpp
    src/transport/io_uring_transport.cpp
)

# Main library links modules
add_library(nexusfix STATIC)
target_link_libraries(nexusfix PUBLIC
    nfx_parser
    nfx_session
    nfx_transport
)
```

### 8.2 Selective Module Compilation

```cmake
option(NFX_ENABLE_SIMD_PARSER "Enable SIMD parser" ON)
option(NFX_ENABLE_IO_URING "Enable io_uring transport" ON)

if(NFX_ENABLE_SIMD_PARSER)
    target_sources(nfx_parser PRIVATE src/parser/simd_parser.cpp)
    target_compile_definitions(nfx_parser PUBLIC NFX_HAS_SIMD_PARSER)
endif()
```

## 9. Versioning

| Component | Version Format | Example |
|-----------|----------------|---------|
| Interface | `MAJOR.MINOR` | `1.0` |
| Implementation | `MAJOR.MINOR.PATCH` | `1.0.3` |
| Protocol (FIX) | `FIX.X.Y` | `FIX.4.4` |

Interface version bump rules:
- **MAJOR**: Breaking change (removed/changed method signature)
- **MINOR**: Additive change (new optional method)

## 10. Summary

| Principle | Enforcement |
|-----------|-------------|
| Interface/Implementation separation | Directory structure |
| Single responsibility | One module = one purpose |
| No cross-module dependencies | CMake + static analysis |
| Compile-time interface verification | `static_assert` + concepts |
| Pluggable implementations | Concept-based design |
