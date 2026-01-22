# TICKET_002: NexusFIX Implementation Plan

**Status**: PLANNING
**Reference**: QuickFIX analysis from `/data/ws/NexusFix/quickfix/quickfix/src/C++/`

## 1. QuickFIX Performance Analysis

### 1.1 Identified Bottlenecks

| File | Line | Issue | Impact |
|------|------|-------|--------|
| `Parser.cpp` | 38-46 | `std::string::find("\0019=")` for each field | O(n) search per field |
| `Parser.cpp` | 51 | `IntConvertor::convert(strLength)` string-to-int | Runtime conversion |
| `Parser.cpp` | 48 | `std::string strLength(buffer, ...)` substring copy | Memory allocation |
| `FieldMap.h` | 83-86 | `std::map<int, vector<FieldMap*>>` | Dynamic allocation + pointer indirection |
| `FieldMap.h` | 108-119 | `setField()` with `findTag()` + insert | O(log n) lookup + O(n) insert |
| `Message.h` | 120 | Constructor from `std::string` | Full message copy |
| `Session.h` | 334 | `Mutex m_mutex` | Lock contention |

### 1.2 QuickFIX Architecture (Reference)

```
QuickFIX Components:
├── Parser          # Stream -> Message boundary detection
├── Message         # Field storage (Header + Body + Trailer)
├── FieldMap        # std::map<tag, Field> storage
├── Field           # tag + string value
├── Session         # State machine (Logon, Heartbeat, etc.)
├── Acceptor        # Server socket listener
├── Initiator       # Client socket connector
└── DataDictionary  # XML-based field definitions
```

---

## 2. NexusFIX Architecture

### 2.1 Design Philosophy

| QuickFIX | NexusFIX | Rationale |
|----------|----------|-----------|
| Runtime XML parsing | Compile-time codegen | Zero runtime overhead |
| `std::string` fields | `std::span<const char>` views | Zero-copy |
| `std::map` lookup | Direct struct access | O(1) access |
| Exception-based errors | `std::expected` | No stack unwinding |
| `std::mutex` locks | Lock-free / coroutines | No contention |

### 2.2 Module Mapping

```
NexusFIX Modules:
├── interfaces/           # Concept-based abstractions
│   ├── i_parser.hpp     # Parser concept
│   ├── i_message.hpp    # Message concept
│   ├── i_session.hpp    # Session concept
│   └── i_transport.hpp  # Transport concept
│
├── parser/               # Zero-copy parsing
│   ├── consteval_parser.hpp   # Compile-time offset calculation
│   ├── simd_parser.hpp        # AVX2 field scanning
│   └── field_extractor.hpp    # Direct pointer access
│
├── messages/             # Fixed-layout message types
│   ├── fix44/
│   │   ├── execution_report.hpp  # 35=8
│   │   ├── new_order_single.hpp  # 35=D
│   │   └── order_cancel.hpp      # 35=F
│   └── common/
│       ├── header.hpp    # BeginString, BodyLength, MsgType, etc.
│       └── trailer.hpp   # CheckSum
│
├── session/              # Coroutine-based state machine
│   ├── session_manager.hpp
│   ├── heartbeat.hpp
│   └── sequence.hpp
│
├── transport/            # Network layer
│   ├── tcp_transport.hpp
│   └── io_uring_transport.hpp  # Linux high-performance
│
├── memory/               # PMR pools
│   ├── message_pool.hpp
│   └── buffer_pool.hpp
│
└── types/                # Strong types
    ├── field_types.hpp   # Price, Qty, etc.
    ├── tag.hpp           # Compile-time tag definitions
    └── error.hpp         # std::expected error types
```

---

## 3. Implementation Phases

### Phase 1: Foundation (Week 1-2)

**Goal**: Core types and compile-time infrastructure

| Task | File | Description |
|------|------|-------------|
| 1.1 | `types/tag.hpp` | Compile-time FIX tag definitions |
| 1.2 | `types/field_types.hpp` | Strong types (Price, Qty, SeqNum) |
| 1.3 | `types/error.hpp` | `std::expected` error types |
| 1.4 | `memory/buffer_pool.hpp` | PMR monotonic buffer |
| 1.5 | `interfaces/i_message.hpp` | Message concept |

**Deliverable**: Type system and memory infrastructure

```cpp
// Example: types/tag.hpp
namespace nfx::tag {

template <int N>
struct Tag {
    static constexpr int value = N;
};

// FIX 4.4 standard tags
using BeginString = Tag<8>;
using BodyLength = Tag<9>;
using MsgType = Tag<35>;
using SenderCompID = Tag<49>;
using TargetCompID = Tag<56>;
using MsgSeqNum = Tag<34>;
using SendingTime = Tag<52>;
using CheckSum = Tag<10>;

// ExecutionReport (35=8) specific
using OrderID = Tag<37>;
using ExecID = Tag<17>;
using ExecType = Tag<150>;
using OrdStatus = Tag<39>;
using Side = Tag<54>;
using LeavesQty = Tag<151>;
using CumQty = Tag<14>;
using AvgPx = Tag<6>;

} // namespace nfx::tag
```

---

### Phase 2: Zero-copy Parser (Week 3-4)

**Goal**: Parse FIX messages without memory allocation

| Task | File | Description |
|------|------|-------------|
| 2.1 | `parser/field_view.hpp` | `std::span<const char>` field view |
| 2.2 | `parser/consteval_parser.hpp` | Compile-time offset calculator |
| 2.3 | `parser/runtime_parser.hpp` | Runtime fallback parser |
| 2.4 | `parser/simd_scanner.hpp` | AVX2 SOH ('\001') scanning |

**Key Innovation**: No `std::string` creation during parsing

```cpp
// Example: parser/field_view.hpp
namespace nfx {

struct FieldView {
    int tag;
    std::span<const char> value;  // Points into original buffer

    // Zero-copy conversions
    [[nodiscard]] constexpr int as_int() const noexcept;
    [[nodiscard]] constexpr double as_double() const noexcept;
    [[nodiscard]] constexpr char as_char() const noexcept;
    [[nodiscard]] constexpr std::string_view as_string() const noexcept;
};

// Parse result: vector of views into original buffer
struct ParseResult {
    std::span<const char> raw;           // Original message
    std::vector<FieldView> fields;       // Views into raw
    bool valid;
};

} // namespace nfx
```

**SIMD Scanner** (AVX2):
```cpp
// Find all SOH ('\001') positions in 32 bytes at once
inline std::vector<size_t> find_soh_positions(std::span<const char> data) {
    const __m256i soh = _mm256_set1_epi8('\001');
    std::vector<size_t> positions;

    for (size_t i = 0; i + 32 <= data.size(); i += 32) {
        __m256i chunk = _mm256_loadu_si256(
            reinterpret_cast<const __m256i*>(data.data() + i));
        __m256i cmp = _mm256_cmpeq_epi8(chunk, soh);
        uint32_t mask = _mm256_movemask_epi8(cmp);

        while (mask) {
            int bit = __builtin_ctz(mask);
            positions.push_back(i + bit);
            mask &= mask - 1;
        }
    }
    return positions;
}
```

---

### Phase 3: Fixed-layout Messages (Week 5-6)

**Goal**: Compile-time message structure definitions

| Task | File | Description |
|------|------|-------------|
| 3.1 | `messages/common/header.hpp` | FIX header struct |
| 3.2 | `messages/common/trailer.hpp` | FIX trailer struct |
| 3.3 | `messages/fix44/execution_report.hpp` | ExecutionReport (35=8) |
| 3.4 | `messages/fix44/new_order_single.hpp` | NewOrderSingle (35=D) |
| 3.5 | `codegen/generate.py` | XML -> C++ struct generator |

**Key Innovation**: Direct struct field access instead of map lookup

```cpp
// Example: messages/fix44/execution_report.hpp
namespace nfx::fix44 {

struct ExecutionReport {
    // Header fields (always present)
    std::string_view begin_string;   // 8
    int body_length;                  // 9
    char msg_type;                    // 35='8'
    std::string_view sender_comp_id; // 49
    std::string_view target_comp_id; // 56
    int msg_seq_num;                  // 34
    std::string_view sending_time;   // 52

    // Body fields
    std::string_view order_id;       // 37
    std::string_view exec_id;        // 17
    char exec_type;                   // 150
    char ord_status;                  // 39
    char side;                        // 54
    double leaves_qty;                // 151
    double cum_qty;                   // 14
    double avg_px;                    // 6

    // Optional fields
    std::optional<std::string_view> text;  // 58
    std::optional<double> last_px;         // 31
    std::optional<double> last_qty;        // 32

    // Trailer
    std::string_view check_sum;      // 10

    // Parse from raw buffer (zero-copy)
    static std::expected<ExecutionReport, ParseError>
    from_buffer(std::span<const char> buffer) noexcept;
};

} // namespace nfx::fix44
```

---

### Phase 4: Session Management (Week 7-8)

**Goal**: Coroutine-based session state machine

| Task | File | Description |
|------|------|-------------|
| 4.1 | `session/state.hpp` | Session state enum |
| 4.2 | `session/sequence.hpp` | Sequence number management |
| 4.3 | `session/heartbeat.hpp` | Heartbeat coroutine |
| 4.4 | `session/session_manager.hpp` | Main session logic |

**Key Innovation**: Coroutine state machine instead of callback spaghetti

```cpp
// Example: session/session_manager.hpp
namespace nfx {

class SessionManager {
public:
    // Coroutine-based session lifecycle
    Task<void> run_session(Transport& transport);

private:
    // State machine states
    Task<void> state_disconnected();
    Task<void> state_logon_sent();
    Task<void> state_logon_received();
    Task<void> state_established();
    Task<void> state_logout_sent();

    // Heartbeat generator
    Generator<Message> heartbeat_generator(
        std::chrono::seconds interval);

    // Message handlers
    Task<void> handle_logon(const fix44::Logon& msg);
    Task<void> handle_heartbeat(const fix44::Heartbeat& msg);
    Task<void> handle_test_request(const fix44::TestRequest& msg);
    Task<void> handle_logout(const fix44::Logout& msg);
};

} // namespace nfx
```

---

### Phase 5: Transport Layer (Week 9-10)

**Goal**: High-performance network I/O

| Task | File | Description |
|------|------|-------------|
| 5.1 | `transport/tcp_transport.hpp` | Standard TCP (portable) |
| 5.2 | `transport/io_uring_transport.hpp` | Linux io_uring |
| 5.3 | `transport/buffer_manager.hpp` | Ring buffer for I/O |

**Key Innovation**: io_uring for kernel bypass on Linux

```cpp
// Example: transport/io_uring_transport.hpp
namespace nfx {

class IoUringTransport {
public:
    // Async read/write using io_uring
    Task<std::span<char>> async_read();
    Task<size_t> async_write(std::span<const char> data);

private:
    io_uring ring_;
    std::array<char, 64 * 1024> recv_buffer_;  // 64KB ring buffer
};

} // namespace nfx
```

---

### Phase 6: Integration & Benchmark (Week 11-12)

**Goal**: End-to-end validation and performance comparison

| Task | File | Description |
|------|------|-------------|
| 6.1 | `benchmarks/parse_benchmark.cpp` | Parse latency comparison |
| 6.2 | `benchmarks/session_benchmark.cpp` | Session throughput |
| 6.3 | `tests/quickfix_compat.cpp` | QuickFIX interop test |
| 6.4 | `examples/simple_client.cpp` | Example initiator |

**Benchmark Targets**:

| Metric | QuickFIX | NexusFIX Target |
|--------|----------|-----------------|
| ExecutionReport parse | 2,000-5,000 ns | < 200 ns |
| Field access | 50-100 ns | < 5 ns |
| Hot path allocations | 10-50 | 0 |
| Session throughput | 50K msg/s | 500K msg/s |

---

## 4. Build System

### 4.1 CMake Structure

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.25)
project(nexusfix VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(NFX_ENABLE_SIMD "Enable SIMD optimizations" ON)
option(NFX_ENABLE_IO_URING "Enable io_uring transport" ON)
option(NFX_BUILD_BENCHMARKS "Build benchmarks" ON)
option(NFX_BUILD_TESTS "Build tests" ON)

# Main library
add_library(nexusfix STATIC
    src/parser/runtime_parser.cpp
    src/session/session_manager.cpp
    src/transport/tcp_transport.cpp
)

target_include_directories(nexusfix PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# SIMD support
if(NFX_ENABLE_SIMD)
    target_compile_options(nexusfix PRIVATE -mavx2)
    target_compile_definitions(nexusfix PUBLIC NFX_HAS_SIMD)
endif()

# io_uring support (Linux only)
if(NFX_ENABLE_IO_URING AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBURING REQUIRED liburing)
    target_link_libraries(nexusfix PRIVATE ${LIBURING_LIBRARIES})
    target_compile_definitions(nexusfix PUBLIC NFX_HAS_IO_URING)
endif()
```

---

## 5. Dependencies

| Dependency | Version | Purpose | Required |
|------------|---------|---------|----------|
| GCC/Clang | 13+ / 17+ | C++23 support | Yes |
| CMake | 3.25+ | Build system | Yes |
| liburing | 2.0+ | io_uring (Linux) | Optional |
| Google Benchmark | 1.8+ | Performance testing | Optional |
| Catch2 | 3.0+ | Unit testing | Optional |

---

## 6. Risk Mitigation

| Risk | Mitigation |
|------|------------|
| C++23 compiler availability | Support GCC 13+ and Clang 17+ |
| io_uring Linux-only | Fallback to standard TCP |
| SIMD portability | Runtime detection + scalar fallback |
| QuickFIX compatibility | Extensive interop testing |

---

## 7. Success Criteria

| Criterion | Measurement |
|-----------|-------------|
| Parse latency | < 200 ns for ExecutionReport |
| Zero allocations | 0 malloc on hot path |
| Throughput | > 500K messages/second |
| Interoperability | Pass QuickFIX acceptor tests |
| Code coverage | > 80% for core modules |
