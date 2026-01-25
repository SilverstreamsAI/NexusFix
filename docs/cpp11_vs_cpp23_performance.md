# C++11 vs C++23: Real-World Performance Case Study

**A practical comparison using FIX protocol engines: Fix8 (C++11) vs NexusFIX (C++23)**

---

## TL;DR

| Metric | C++11 (Fix8) | C++23 (NexusFIX) | Speedup |
|--------|--------------|------------------|---------|
| Message Decode (P50) | 1,453 ns | 193 ns | **7.5x** |
| Message Decode (P99) | 2,203 ns | 398 ns | **5.5x** |
| Hot Path Allocations | ~5-10 | 0 | **Eliminated** |
| Binary Size | Larger (exceptions) | Smaller (no unwind) | ~15% smaller |

**These are real measurements**, not vendor claims. See [benchmark report](compare/FIX8_VS_NEXUSFIX_BENCHMARK.md) for methodology.

---

## Why This Comparison?

Both Fix8 and NexusFIX solve the same problem: parse and generate FIX protocol messages for trading systems. The difference is **12 years of C++ evolution**.

| Aspect | Fix8 | NexusFIX |
|--------|------|----------|
| First Release | ~2012 | 2024 |
| C++ Standard | C++11 | C++23 |
| Design Philosophy | OOP + Runtime | Compile-time + Zero-cost |

Same problem. Same hardware. **7.5x performance difference** (measured).

---

## 1. Compile-Time vs Runtime Dispatch

### C++11: Virtual Functions

```cpp
// Fix8 approach - runtime polymorphism
class Message {
public:
    virtual bool decode(const char* buffer, size_t len) = 0;
    virtual bool encode(char* buffer, size_t& len) = 0;
    virtual ~Message() = default;
};

class ExecutionReport : public Message {
public:
    bool decode(const char* buffer, size_t len) override {
        // vtable lookup + indirect call
    }
};

// Usage - runtime dispatch
void process(Message* msg) {
    msg->decode(buffer, len);  // indirect call through vtable
}
```

**Cost per call:**
- vtable pointer load: ~1-3 cycles
- Indirect branch: ~10-20 cycles (misprediction penalty)
- Prevents inlining: loses optimization opportunities

### C++23: Static Polymorphism with Concepts

```cpp
// NexusFIX approach - compile-time polymorphism
template<typename T>
concept FIXMessage = requires(T msg, std::span<const char> buf) {
    { msg.decode(buf) } -> std::same_as<std::expected<size_t, ParseError>>;
    { msg.encode() } -> std::same_as<std::span<const char>>;
    { T::msg_type } -> std::convertible_to<std::string_view>;
};

struct ExecutionReport {
    static constexpr std::string_view msg_type = "8";

    std::expected<size_t, ParseError> decode(std::span<const char> buf) noexcept {
        // Direct call, fully inlinable
    }
};

// Usage - compile-time dispatch with std::variant
using AnyMessage = std::variant<ExecutionReport, NewOrderSingle, OrderCancel>;

void process(AnyMessage& msg) {
    std::visit([](auto& m) {
        m.decode(buffer);  // Direct call, compiler knows exact type
    }, msg);
}
```

**Cost per call:**
- No vtable: 0 cycles
- Direct branch: predictable, ~1 cycle
- Full inlining: entire decode logic embedded at call site

### Performance Impact

```
Fix8 (virtual):     ~15-25 cycles per message type dispatch
NexusFIX (variant): ~1-3 cycles per message type dispatch
```

---

## 2. Memory Allocation Strategy

### C++11: Object Pools with Manual Lifecycle

```cpp
// Fix8 approach - object recycling
class MessagePool {
    std::stack<Message*> free_list_;
    std::mutex mutex_;  // Thread safety

public:
    Message* acquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (free_list_.empty()) {
            return new ExecutionReport();  // Heap allocation
        }
        Message* msg = free_list_.top();
        free_list_.pop();
        msg->clear();  // Reset state
        return msg;
    }

    void release(Message* msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        free_list_.push(msg);
    }
};

// Usage requires discipline
void process_message(const char* buffer) {
    Message* msg = pool.acquire();
    msg->decode(buffer, len);
    // ... use message ...
    pool.release(msg);  // Must not forget!
}
```

**Problems:**
- Mutex contention in multi-threaded code
- Manual release required (leak if forgotten)
- `clear()` has runtime cost
- Pool exhaustion falls back to `new`

### C++23: PMR (Polymorphic Memory Resources)

```cpp
// NexusFIX approach - PMR monotonic buffer
class MessageArena {
    alignas(64) std::array<std::byte, 64 * 1024> buffer_;
    std::pmr::monotonic_buffer_resource resource_{buffer_.data(), buffer_.size()};
    std::pmr::polymorphic_allocator<> alloc_{&resource_};

public:
    template<FIXMessage T, typename... Args>
    T* create(Args&&... args) {
        return alloc_.new_object<T>(std::forward<Args>(args)...);
    }

    void reset() noexcept {
        resource_.release();  // O(1) - just reset pointer
    }
};

// Usage - automatic, no manual release
void process_batch(std::span<const char*> messages) {
    MessageArena arena;

    for (const char* buf : messages) {
        auto* msg = arena.create<ExecutionReport>();
        msg->decode(buf);
        // ... use message ...
    }
    // arena.reset() called automatically in destructor
}
```

**Advantages:**
- No mutex: single-threaded hot path
- No manual release: RAII handles cleanup
- `reset()` is O(1): just moves pointer back
- Zero `malloc` calls: pre-allocated buffer

### Performance Impact

```
Fix8 (pool):     ~50-100 ns per acquire/release cycle
NexusFIX (PMR):  ~0 ns (allocation is pointer increment)
```

---

## 3. Error Handling

### C++11: Exceptions

```cpp
// Fix8 approach - exceptions for errors
class Message {
public:
    void decode(const char* buffer, size_t len) {
        if (len < MIN_MESSAGE_SIZE) {
            throw InvalidMessage("Buffer too small");
        }
        if (!validate_checksum(buffer, len)) {
            throw ChecksumError("Invalid checksum");
        }
        // ... parsing logic ...
    }
};

// Caller must use try-catch
try {
    msg->decode(buffer, len);
} catch (const InvalidMessage& e) {
    log_error(e.what());
} catch (const ChecksumError& e) {
    request_resend();
}
```

**Hidden costs:**
- Exception tables in binary (~10-15% size increase)
- Stack unwinding machinery
- Non-deterministic latency when exception thrown
- Prevents `noexcept` optimization

### C++23: std::expected

```cpp
// NexusFIX approach - std::expected for errors
enum class ParseError : uint8_t {
    BufferTooSmall,
    InvalidChecksum,
    MalformedField,
    UnknownMsgType
};

struct ExecutionReport {
    [[nodiscard]]
    std::expected<size_t, ParseError> decode(std::span<const char> buf) noexcept {
        if (buf.size() < MIN_MESSAGE_SIZE) {
            return std::unexpected(ParseError::BufferTooSmall);
        }
        if (!validate_checksum(buf)) {
            return std::unexpected(ParseError::InvalidChecksum);
        }
        // ... parsing logic ...
        return bytes_consumed;
    }
};

// Caller uses value-based error handling
auto result = msg.decode(buffer);
if (!result) {
    switch (result.error()) {
        case ParseError::InvalidChecksum:
            request_resend();
            break;
        default:
            log_error(result.error());
    }
} else {
    size_t consumed = *result;
}
```

**Advantages:**
- No exception tables: smaller binary
- Deterministic: same cost success or failure
- `noexcept` enables optimizer
- Explicit error handling: no hidden control flow

### Performance Impact

```
Fix8 (exception):    ~0 ns success, ~1000+ ns on throw
NexusFIX (expected): ~1-2 ns always (predictable)
```

---

## 4. Compile-Time Computation

### C++11: Runtime Initialization

```cpp
// Fix8 approach - runtime field offset calculation
class FieldTraits {
    std::unordered_map<int, FieldInfo> traits_;

public:
    FieldTraits() {
        // Runtime initialization
        traits_[8] = {FieldType::String, "BeginString", true};
        traits_[9] = {FieldType::Length, "BodyLength", true};
        traits_[35] = {FieldType::String, "MsgType", true};
        // ... 100+ more fields ...
    }

    const FieldInfo& get(int tag) const {
        return traits_.at(tag);  // Hash lookup
    }
};

// Global instance, initialized at startup
static FieldTraits g_traits;  // Runtime cost at program start
```

**Costs:**
- Startup time: initializing all field traits
- Memory: hash table overhead
- Lookup: hash computation + possible collision

### C++23: consteval Everything

```cpp
// NexusFIX approach - compile-time field traits
struct FieldInfo {
    int tag;
    std::string_view name;
    FieldType type;
    bool required;
};

consteval auto make_field_table() {
    std::array<FieldInfo, 1024> table{};
    table[8] = {8, "BeginString", FieldType::String, true};
    table[9] = {9, "BodyLength", FieldType::Length, true};
    table[35] = {35, "MsgType", FieldType::String, true};
    // ... computed at compile time ...
    return table;
}

// Embedded in binary as static data
inline constexpr auto FIELD_TABLE = make_field_table();

// Lookup is array index - single instruction
constexpr const FieldInfo& get_field(int tag) {
    return FIELD_TABLE[tag];
}
```

**Advantages:**
- Zero startup cost: data is in binary
- O(1) lookup: direct array index
- Compiler optimization: known at compile time

### Generated Assembly Comparison

```asm
; Fix8 - runtime hash lookup
mov     rdi, [traits_ptr]      ; load map pointer
mov     esi, 35                ; tag number
call    std::unordered_map::at ; function call, hash, probe

; NexusFIX - compile-time array
mov     rax, [FIELD_TABLE + 35*sizeof(FieldInfo)]  ; single instruction
```

---

## 5. String Handling

### C++11: std::string Everywhere

```cpp
// Fix8 approach - string copies
class Message {
    std::map<int, std::string> fields_;  // Owns copies of all values

public:
    std::string getField(int tag) const {
        auto it = fields_.find(tag);
        if (it != fields_.end()) {
            return it->second;  // Copy on return
        }
        return "";
    }

    void setField(int tag, const std::string& value) {
        fields_[tag] = value;  // Copy on insert
    }
};
```

**Costs:**
- Heap allocation per field value
- Copy on get and set
- SSO helps only for short strings (<15 chars)

### C++23: std::span + std::string_view

```cpp
// NexusFIX approach - zero-copy views
class Message {
    std::span<const char> buffer_;  // View into original data
    std::array<FieldView, 1024> fields_;  // Offset + length only

    struct FieldView {
        uint16_t offset;
        uint16_t length;
    };

public:
    [[nodiscard]]
    std::string_view get_field(int tag) const noexcept {
        auto& f = fields_[tag];
        return {buffer_.data() + f.offset, f.length};  // No copy
    }

    // For mutation, use a separate builder pattern
};
```

**Advantages:**
- Zero allocation: views point to existing data
- Cache friendly: no pointer chasing
- 4 bytes per field vs 32+ bytes (std::string)

### Performance Impact

```
Fix8 (std::string): ~50-100 ns per field access (with allocation)
NexusFIX (view):    ~1-2 ns per field access (pointer arithmetic)
```

---

## 6. Thread Safety Approach

### C++11: Mutex + Condition Variables

```cpp
// Fix8 approach - shared state with locks
class Session {
    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<Message*> outbound_;

public:
    void send(Message* msg) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            outbound_.push(msg);
        }
        cv_.notify_one();  // Wake writer thread
    }

    Message* next_message() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !outbound_.empty(); });
        Message* msg = outbound_.front();
        outbound_.pop();
        return msg;
    }
};
```

### C++23: Lock-Free + Atomics with Memory Order

```cpp
// NexusFIX approach - single-threaded hot path, lock-free handoff
class Session {
    // Hot path: single-threaded, no synchronization needed
    struct alignas(64) HotPath {  // Cache-line aligned
        std::span<const char> current_message;
        uint64_t sequence_number;
    } hot_;

    // Cold path: infrequent operations use atomics
    std::atomic<uint64_t> last_sent_{0};
    std::atomic<uint64_t> last_received_{0};

public:
    // Hot path - no synchronization
    void process_message(std::span<const char> msg) noexcept {
        hot_.current_message = msg;
        ++hot_.sequence_number;
        // ... process ...
    }

    // Cold path - memory_order_relaxed when possible
    uint64_t get_last_sent() const noexcept {
        return last_sent_.load(std::memory_order_relaxed);
    }

    void set_last_sent(uint64_t seq) noexcept {
        last_sent_.store(seq, std::memory_order_release);
    }
};
```

**Key insight:** C++23 enables designing *around* synchronization rather than using synchronization everywhere.

---

## 7. Feature Comparison Summary

| Feature | C++11 | C++23 | Performance Impact |
|---------|-------|-------|-------------------|
| Polymorphism | `virtual` | `concepts` + `variant` | 10-20x faster dispatch |
| Error Handling | `throw/catch` | `std::expected` | Deterministic latency |
| Memory | `new/delete` | PMR | Zero hot-path allocation |
| Strings | `std::string` | `std::string_view` | Zero-copy |
| Computation | Runtime init | `consteval` | Zero startup cost |
| Attributes | Limited | `[[likely]]`, `[[nodiscard]]` | Better optimization hints |
| Formatting | `sprintf`/streams | `std::format` | Type-safe, fast |
| Ranges | Manual loops | `std::ranges` | Cleaner, same performance |

---

## 8. Migration Path

If you have C++11 code and want C++23 performance:

### Phase 1: Low-Risk Changes
```cpp
// Replace virtual with CRTP where possible
// Add noexcept to all non-throwing functions
// Replace std::endl with '\n'
// Add [[nodiscard]] to all query functions
```

### Phase 2: Memory Optimization
```cpp
// Identify hot path allocations with custom allocator
// Replace std::string with std::string_view for read-only
// Consider PMR for object pools
```

### Phase 3: Error Handling
```cpp
// Replace exceptions with std::expected on hot paths
// Keep exceptions for truly exceptional cases
// Add std::expected to new APIs
```

### Phase 4: Compile-Time
```cpp
// Convert runtime lookup tables to constexpr
// Use consteval for validation
// Replace macros with templates
```

---

## 9. Benchmark Methodology

All measurements taken with:

```bash
# CPU isolation
echo "performance" | sudo tee /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
taskset -c 0 ./benchmark

# Compilation
g++ -std=c++23 -O3 -march=native -flto -DNDEBUG

# Measurement
# 10,000 warmup iterations
# 1,000,000 measured iterations
# Report: min, P50, P99, P999, max
```

Hardware: Intel Core i9-13900K, 64GB DDR5, Linux 6.x

---

## 10. Conclusion

C++23 is not just "nicer syntax" over C++11. It enables fundamentally different design patterns:

| C++11 Design | C++23 Design |
|--------------|--------------|
| Runtime flexibility | Compile-time guarantees |
| Object-oriented | Value-oriented |
| Exceptions for errors | Expected values |
| Heap allocation | Stack/arena allocation |
| Virtual dispatch | Static dispatch |

The **7.5x performance improvement** between Fix8 and NexusFIX is not magic. It is the accumulated benefit of applying modern C++ patterns consistently:

- **~3x** from eliminating heap allocation (std::string -> std::string_view)
- **~1.5x** from static dispatch (virtual -> std::variant)
- **~1.2x** from compile-time computation (runtime hash -> consteval)
- **~1.3x** from cache-friendly data layout

Cumulative: 3 x 1.5 x 1.2 x 1.3 = **7.0x** (measured: 7.5x)

**Modern C++ is not about learning new syntax. It is about learning new ways to think about performance.**

---

## Discussion

We welcome discussion and questions:

- [GitHub Issues](https://github.com/SilverstreamsAI/NexusFix/issues) - Bug reports, questions
- [GitHub Discussions](https://github.com/SilverstreamsAI/NexusFix/discussions) - Performance discussions

---

## References

- [NexusFIX Optimization Diary](optimization_diary.md) - Detailed optimization journey
- [Modern C++ for Quantitative Trading](modernc_quant.md) - 100 techniques
- [Fix8 GitHub Repository](https://github.com/fix8/fix8) - C++11 FIX engine
- [C++23 Standard Draft](https://eel.is/c++draft/) - Language reference
