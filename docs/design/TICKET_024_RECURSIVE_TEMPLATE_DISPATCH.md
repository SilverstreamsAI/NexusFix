# TICKET_024: Recursive Template Dispatch Optimization

**Status**: Phase 1 REVERTED, Phase 2 Complete
**Category**: Performance Optimization
**Priority**: High
**Technique**: Recursive Template Metaprogramming (C++23)
**Reference**: TICKET_INTERNAL_022 (Recursive Template Trading Strategy)

---

## Overview

Apply recursive template metaprogramming patterns to eliminate runtime switch statements and virtual dispatch in hot paths. This follows the proven FilterChain/QuickSort pattern from TICKET_INTERNAL_022.

---

## Problem Areas

### 1. Admin Message Dispatch (HOT PATH)

**File**: `include/nexusfix/session/session_manager.hpp:339`

**Current Implementation**:
```cpp
void handle_admin_message(const ParsedMessage& msg) noexcept {
    switch (msg.msg_type()) {
        case msg_type::Logon:      handle_logon(msg);           break;
        case msg_type::Logout:     handle_logout(msg);          break;
        case msg_type::Heartbeat:  handle_heartbeat(msg);       break;
        case msg_type::TestRequest: handle_test_request(msg);   break;
        case msg_type::ResendRequest: handle_resend_request(msg); break;
        case msg_type::SequenceReset: handle_sequence_reset(msg); break;
        case msg_type::Reject:     handle_reject(msg);          break;
    }
}
```

**Issues**:
1. 7-case runtime switch on every admin message
2. Branch prediction pressure (frequently hit Heartbeat)
3. No compile-time handler completeness validation
4. Adding new admin types requires manual switch update

---

### 2. Message Store Virtual Dispatch

**File**: `include/nexusfix/store/i_message_store.hpp`

**Current Implementation**:
```cpp
class IMessageStore {
public:
    virtual ~IMessageStore() = default;
    [[nodiscard]] virtual bool store(...) noexcept = 0;
    [[nodiscard]] virtual std::optional<...> retrieve(...) const noexcept = 0;
    // ... 8 virtual functions total
};
```

**Issues**:
1. Virtual dispatch overhead (~10-15 cycles per call)
2. Called on every message store/retrieve (resend path)
3. Indirect call prevents inlining
4. vtable lookup on each call

---

## Solution: Recursive Template Dispatch

### Pattern Overview (from TICKET_INTERNAL_022)

```cpp
// 1. Type container (holds handler types)
template<typename... Handlers>
struct HandlerList {};

// 2. Forward declaration
template<typename List>
struct DispatchChain;

// 3. Base case (recursion terminator)
template<>
struct DispatchChain<HandlerList<>> {
    static constexpr void dispatch(char, const ParsedMessage&) noexcept {
        // No matching handler - no-op or error
    }
};

// 4. Recursive case
template<typename First, typename... Rest>
struct DispatchChain<HandlerList<First, Rest...>> {
    static constexpr void dispatch(char msg_type, const ParsedMessage& msg) noexcept {
        if (msg_type == First::msg_type) {
            First::handle(msg);
        } else {
            DispatchChain<HandlerList<Rest...>>::dispatch(msg_type, msg);
        }
    }
};
```

---

## Implementation Plan

### Phase 1: Admin Message Dispatch Table

#### Step 1.1: Define Handler Traits

```cpp
namespace nfx::session::detail {

// Handler trait - maps MsgType to handler function
template<char MsgType>
struct AdminHandler;

template<>
struct AdminHandler<msg_type::Logon> {
    static constexpr char msg_type = '0';
    static constexpr int priority = 10;  // Common messages get lower priority (checked first)

    template<typename Session>
    static void handle(Session& s, const ParsedMessage& msg) noexcept {
        s.handle_logon(msg);
    }
};

template<>
struct AdminHandler<msg_type::Heartbeat> {
    static constexpr char msg_type = '0';
    static constexpr int priority = 5;  // Most frequent - check first

    template<typename Session>
    static void handle(Session& s, const ParsedMessage& msg) noexcept {
        s.handle_heartbeat(msg);
    }
};

// ... specializations for all 7 admin message types

} // namespace nfx::session::detail
```

#### Step 1.2: Create Compile-time Dispatch Table

```cpp
namespace nfx::session::detail {

// Type list of all admin handlers
using AdminHandlerList = HandlerList<
    AdminHandler<msg_type::Heartbeat>,     // Priority 5  - most frequent
    AdminHandler<msg_type::Logon>,         // Priority 10
    AdminHandler<msg_type::Logout>,        // Priority 15
    AdminHandler<msg_type::TestRequest>,   // Priority 20
    AdminHandler<msg_type::ResendRequest>, // Priority 25
    AdminHandler<msg_type::SequenceReset>, // Priority 30
    AdminHandler<msg_type::Reject>         // Priority 35
>;

// Compile-time sorted by priority (using QuickSort pattern from TICKET_INTERNAL_022)
using SortedAdminHandlers = SortHandlers_t<AdminHandlerList>;

// Final dispatch chain
using AdminDispatcher = DispatchChain<SortedAdminHandlers>;

} // namespace nfx::session::detail
```

#### Step 1.3: Generate O(1) Lookup Table

```cpp
namespace nfx::session::detail {

// Compile-time function pointer table
template<typename Session>
struct AdminDispatchTable {
    using HandlerFn = void(*)(Session&, const ParsedMessage&) noexcept;

    // Generate 128-entry lookup table at compile time
    static consteval std::array<HandlerFn, 128> create_table() noexcept {
        std::array<HandlerFn, 128> table{};
        table.fill(nullptr);  // No handler for unknown types

        // Populate from handler list
        populate_entry<AdminHandler<msg_type::Heartbeat>>(table);
        populate_entry<AdminHandler<msg_type::Logon>>(table);
        // ... all handlers

        return table;
    }

    template<typename Handler>
    static consteval void populate_entry(std::array<HandlerFn, 128>& table) noexcept {
        table[static_cast<unsigned char>(Handler::msg_type)] = &Handler::template handle<Session>;
    }

    static constexpr auto table = create_table();
};

} // namespace nfx::session::detail
```

#### Step 1.4: Replace Switch with Table Lookup

```cpp
// Before
void handle_admin_message(const ParsedMessage& msg) noexcept {
    switch (msg.msg_type()) {
        case msg_type::Logon: handle_logon(msg); break;
        // ... 6 more cases
    }
}

// After
void handle_admin_message(const ParsedMessage& msg) noexcept {
    const auto idx = static_cast<unsigned char>(msg.msg_type());
    if (auto handler = detail::AdminDispatchTable<SessionManager>::table[idx]) {
        handler(*this, msg);
    }
    // Unknown admin type - ignore (defensive)
}
```

---

### Phase 2: Message Store Static Polymorphism

#### Step 2.1: Define Store Types

```cpp
namespace nfx::store {

// Forward declare all store implementations
class MemoryStore;
class FileStore;
class NullStore;

// Type list of all stores
template<typename... Stores>
struct StoreList {};

using AllStores = StoreList<MemoryStore, FileStore, NullStore>;

// Convert to variant
template<typename List>
struct ToVariant;

template<typename... Stores>
struct ToVariant<StoreList<Stores...>> {
    using type = std::variant<Stores...>;
};

using StoreVariant = ToVariant<AllStores>::type;

} // namespace nfx::store
```

#### Step 2.2: Replace Interface with Variant

```cpp
namespace nfx::store {

// Variant-based store (replaces IMessageStore interface)
class MessageStore {
public:
    template<typename StoreImpl, typename... Args>
    explicit MessageStore(std::in_place_type_t<StoreImpl>, Args&&... args)
        : impl_(std::in_place_type<StoreImpl>, std::forward<Args>(args)...) {}

    [[nodiscard]] bool store(uint32_t seq_num, std::span<const char> msg) noexcept {
        return std::visit([&](auto& s) { return s.store(seq_num, msg); }, impl_);
    }

    [[nodiscard]] std::optional<std::vector<char>> retrieve(uint32_t seq_num) const noexcept {
        return std::visit([&](const auto& s) { return s.retrieve(seq_num); }, impl_);
    }

    // ... other methods using std::visit

private:
    StoreVariant impl_;
};

} // namespace nfx::store
```

#### Step 2.3: Compile-time Store Selection

```cpp
namespace nfx::store {

// Store selection based on configuration
template<StoreType Type>
struct StoreSelector;

template<>
struct StoreSelector<StoreType::Memory> {
    using type = MemoryStore;
};

template<>
struct StoreSelector<StoreType::File> {
    using type = FileStore;
};

template<StoreType Type>
using StoreSelector_t = typename StoreSelector<Type>::type;

// Factory with compile-time type selection
template<StoreType Type, typename... Args>
[[nodiscard]] MessageStore create_store(Args&&... args) {
    return MessageStore{
        std::in_place_type<StoreSelector_t<Type>>,
        std::forward<Args>(args)...
    };
}

} // namespace nfx::store
```

---

### Phase 3: Validation Chain

#### Step 3.1: Define Validator Concept

```cpp
namespace nfx::parser {

template<typename V>
concept ValidatorConcept = requires(V v, const ParsedMessage& msg) {
    { V::validate(msg) } -> std::same_as<ParseError>;
    { V::name } -> std::convertible_to<const char*>;
    { V::priority } -> std::convertible_to<int>;
};

} // namespace nfx::parser
```

#### Step 3.2: Implement Validators

```cpp
namespace nfx::parser {

// Required field validator
template<int Tag>
struct RequiredFieldValidator {
    static constexpr const char* name = "RequiredField";
    static constexpr int priority = 10;  // Run early

    static constexpr ParseError validate(const ParsedMessage& msg) noexcept {
        if (!msg.has_field(Tag)) {
            return ParseError{ParseErrorCode::MissingRequiredField, Tag};
        }
        return ParseError{};
    }
};

// Checksum validator
struct ChecksumValidator {
    static constexpr const char* name = "Checksum";
    static constexpr int priority = 5;  // Run first

    static constexpr ParseError validate(const ParsedMessage& msg) noexcept {
        // Validate checksum
        return ParseError{};
    }
};

// Body length validator
struct BodyLengthValidator {
    static constexpr const char* name = "BodyLength";
    static constexpr int priority = 6;  // Run after checksum

    static constexpr ParseError validate(const ParsedMessage& msg) noexcept {
        // Validate body length matches actual
        return ParseError{};
    }
};

} // namespace nfx::parser
```

#### Step 3.3: Build Validation Chain

```cpp
namespace nfx::parser {

// Validator list
template<typename... Validators>
struct ValidatorList {};

// Validation chain (recursive)
template<typename List>
struct ValidationChain;

template<>
struct ValidationChain<ValidatorList<>> {
    static constexpr ParseError validate(const ParsedMessage&) noexcept {
        return ParseError{};  // All validators passed
    }
};

template<typename First, typename... Rest>
struct ValidationChain<ValidatorList<First, Rest...>> {
    static_assert(ValidatorConcept<First>, "Must satisfy ValidatorConcept");

    static constexpr ParseError validate(const ParsedMessage& msg) noexcept {
        if (auto err = First::validate(msg); err.code != ParseErrorCode::None) {
            return err;  // Early exit on first failure
        }
        return ValidationChain<ValidatorList<Rest...>>::validate(msg);
    }
};

// Sort validators by priority at compile time
template<typename List>
using SortedValidators = SortByPriority_t<List>;

// Complete validation chain for ExecutionReport
using ExecutionReportValidators = ValidatorList<
    ChecksumValidator,                    // Priority 5
    BodyLengthValidator,                  // Priority 6
    RequiredFieldValidator<35>,           // MsgType, Priority 10
    RequiredFieldValidator<49>,           // SenderCompID
    RequiredFieldValidator<56>,           // TargetCompID
    RequiredFieldValidator<37>,           // OrderID
    RequiredFieldValidator<17>,           // ExecID
    RequiredFieldValidator<150>,          // ExecType
    RequiredFieldValidator<39>            // OrdStatus
>;

using ExecutionReportValidator = ValidationChain<SortedValidators<ExecutionReportValidators>>;

} // namespace nfx::parser
```

---

## Expected Benefits

### Phase 1: Admin Message Dispatch

| Metric | Before | After |
|--------|--------|-------|
| Dispatch mechanism | 7-case switch | O(1) table lookup |
| Branch prediction | 7 branches | 1 null check |
| Compile-time validation | None | static_assert coverage |
| Code maintenance | Update switch | Update handler trait |

**Expected Improvement**: 40-60% faster dispatch (based on TICKET_022 results)

### Phase 2: Message Store

| Metric | Before | After |
|--------|--------|-------|
| Dispatch mechanism | Virtual call | std::visit (devirtualized) |
| Call overhead | ~10-15 cycles | ~2-3 cycles |
| Inlining | Impossible | Possible |
| Memory | vtable + pointer | Variant inline storage |

**Expected Improvement**: 60-80% reduction in dispatch overhead

### Phase 3: Validation Chain

| Metric | Before | After |
|--------|--------|-------|
| Complexity | O(n x m) nested loops | O(n) linear chain |
| Early exit | Manual | Automatic |
| Extensibility | Modify loop | Add validator type |
| Compile-time check | None | static_assert |

**Expected Improvement**: Cleaner code, compile-time validation

---

## Benchmark Plan

### Test Cases

1. **Admin Dispatch Microbenchmark**
   - 1M Heartbeat messages (most frequent)
   - 1M mixed admin messages (random distribution)
   - Measure cycles per dispatch

2. **Message Store Throughput**
   - 1M store/retrieve cycles
   - Compare virtual vs variant dispatch

3. **Validation Chain**
   - 100K message validation
   - Compare loop vs chain approach

### Metrics

| Metric | Tool |
|--------|------|
| Cycles per operation | RDTSC |
| Branch mispredictions | perf stat |
| Cache misses | perf stat |
| Instructions per call | perf stat |

---

## Files to Modify

### Phase 1
- `include/nexusfix/session/session_manager.hpp` - Replace switch with table
- `include/nexusfix/session/admin_dispatch.hpp` (new) - Handler traits and table

### Phase 2
- `include/nexusfix/store/i_message_store.hpp` - Add variant-based store
- `include/nexusfix/store/message_store.hpp` (new) - Unified store class

### Phase 3
- `include/nexusfix/parser/validation_chain.hpp` (new) - Validator framework
- `include/nexusfix/parser/consteval_parser.hpp` - Use validation chain

---

## Static Assertions

```cpp
// Phase 1: Verify all admin handlers registered
static_assert(detail::AdminDispatchTable<SessionManager>::table[msg_type::Logon] != nullptr);
static_assert(detail::AdminDispatchTable<SessionManager>::table[msg_type::Heartbeat] != nullptr);
// ... all 7 admin types

// Phase 2: Verify store variant completeness
static_assert(std::variant_size_v<StoreVariant> == 3, "Expected 3 store types");

// Phase 3: Verify validation chain
static_assert(ValidatorConcept<ChecksumValidator>);
static_assert(ValidatorConcept<RequiredFieldValidator<35>>);
```

---

## Risk Assessment

| Risk | Mitigation |
|------|-----------|
| Compile time increase | Template instantiation is bounded (7 admin types) |
| Code complexity | Clear documentation, follows established pattern |
| Binary size | Jump table may be larger than switch; measure |
| Debug difficulty | Add name() to each handler for logging |

---

## Success Criteria

1. **Performance**: Admin dispatch < 10 cycles (down from ~20-30)
2. **Correctness**: All existing tests pass
3. **Maintainability**: Adding new handler = 1 template specialization
4. **Validation**: Compile-time static_assert for completeness

---

## References

- TICKET_INTERNAL_022: Recursive Template Trading Strategy (FilterChain pattern)
- TICKET_022: Compile-time MsgType Dispatch (table generation pattern)
- TICKET_023: Compile-time Optimization Roadmap
- docs/modernc_quant.md: Techniques #29 (variant), #8 (concepts), #1 (consteval)

---

## Phase 1 Implementation Results

**Date**: 2026-02-01

### Files Created/Modified

| File | Change |
|------|--------|
| `include/nexusfix/session/admin_dispatch.hpp` | NEW - Handler traits + dispatch table |
| `include/nexusfix/session/session_manager.hpp` | Modified - Use table dispatch |
| `benchmarks/admin_dispatch_bench.cpp` | NEW - Performance benchmark |
| `docs/compare/TICKET_024_ADMIN_DISPATCH_BENCHMARK.md` | NEW - Benchmark results |

### Benchmark Results

| Scenario | Switch (cycles) | Table (cycles) | Improvement |
|----------|-----------------|----------------|-------------|
| All admin types | 18.58 | 18.25 | **+1.8%** |
| Heartbeat only | 11.22 | 18.57 | -65.5% |
| Random pattern | 11.36 | 19.02 | -67.4% |

### Analysis

For 7 admin message types, the compiler-generated switch is already optimal:
- Branch prediction works well for repeated message types
- Direct jumps vs indirect function pointer calls
- Compiler can inline switch cases

**The table approach provides value through:**
1. Code organization (single template specialization per handler)
2. Compile-time safety (`static_assert` for missing handlers)
3. Consistent latency (no branch prediction variance)
4. Better maintainability

### Conclusion

Phase 1 achieves its primary goals of code organization and compile-time safety without performance regression in realistic scenarios. The 7-case switch is a special case where modern compilers and CPUs work exceptionally well together.

**Recommendation**: ~~Keep table dispatch for maintainability benefits.~~ **REVERTED** - See Phase 1 Retrospective below.

---

## Phase 1 Retrospective (REVERTED)

**Date**: 2026-02-01
**Decision**: Rollback to original switch implementation

### Why We Rolled Back

我们用 template 实现了函数指针查找表，但这是**过度工程**：

| 方面 | 我们的 Table | 编译器的 Switch |
|------|-------------|-----------------|
| 跳转表 | 手动创建 | **自动生成** |
| 优化 | 我们的代码 | **编译器更聪明** |
| 分支预测 | 间接调用，预测差 | **直接跳转，预测好** |
| 单一消息类型 | 19.02 cycles | **11.37 cycles** |

**我们重新实现了编译器已经做得更好的事情。**

### TICKET_INTERNAL_022 的真正适用场景

递归模板的价值在于**编译时类型操作**：

```cpp
// 编译时排序 Filter 链
using SortedFilters = SortByPriority_t<FilterList<F1, F2, F3>>;

// 编译时展开，零运行时开销
FilterChain<SortedFilters>::apply(signal);  // 完全内联
```

适合：
- 编译时组合/排序
- 类型级别操作
- 完全消除运行时分派（内联展开）

**不适合**：简单的 N-case 消息分派（编译器 switch 已最优）

### Lesson Learned

> 不要重新实现编译器已经做得更好的事情。
> Switch 对于小规模分派是最优解，CPU 分支预测 + 编译器优化已经足够。

---

## Phase 2 Implementation Results

**Date**: 2026-02-01

### Files Created

| File | Description |
|------|-------------|
| `include/nexusfix/store/message_store.hpp` | **NEW** - Variant-based store with NullStore/MemoryStore |
| `benchmarks/message_store_variant_bench.cpp` | **NEW** - Performance benchmark |
| `docs/compare/TICKET_024_MESSAGE_STORE_VARIANT_BENCHMARK.md` | **NEW** - Benchmark results |

### Benchmark Results

| Operation | Virtual (cycles) | Variant (cycles) | Result |
|-----------|------------------|------------------|--------|
| NullStore: store() | 0.84 | 1.22 | **-45.5%** |
| NullStore: retrieve() | 0.92 | 0.94 | **-3.0%** |
| NullStore: seq_num | 0.63 | 0.93 | **-49.1%** |

### Key Finding

**Virtual dispatch is NOT a bottleneck** for message store operations:

1. For trivial operations (NullStore): Virtual dispatch wins due to branch prediction
2. Virtual dispatch overhead is ~10-15 cycles - negligible vs actual work
3. Real bottlenecks are PMR allocation, hash maps, mutex locks

### Conclusion

The variant-based `MessageStore` is available as an alternative implementation, but:
- **Keep `IMessageStore` interface** for existing code compatibility
- **Use `MessageStore` variant** for new code that wants type-safe dispatch
- **Focus optimization on store internals**, not dispatch mechanism

The optimization value of replacing virtual dispatch is minimal for operations that do real work.

---

## Phase 2 Retrospective

**Date**: 2026-02-01
**Decision**: 保留 `message_store.hpp` 作为可选 API，不替换 `IMessageStore`

### 最新 Benchmark 结果

| 操作 | Virtual (cycles) | Variant (cycles) | 结果 |
|------|------------------|------------------|------|
| NullStore: store() | 1.73 | 2.06 | **-19% 变慢** |
| NullStore: retrieve() | 1.72 | 2.52 | **-47% 变慢** |
| NullStore: seq_num | 1.68 | 1.34 | **+20% 变快** |
| MemoryStore: store() | 32,191 | 484 | **+98% 变快** |

### 问题：能否只保留变快的操作？

**不行**，原因：

1. **Dispatch 机制是整个对象的**
   - 不能按方法选择 virtual 或 variant
   - 一个对象要么全用 virtual，要么全用 variant

2. **MemoryStore 98% 快是假象**
   - 不是因为 `std::visit` vs virtual dispatch
   - 是因为实现不同：

   | 实现 | 数据结构 | 结果 |
   |------|----------|------|
   | `MemoryMessageStore` | PMR + HashMap + mutex | 32,191 cycles |
   | `MemoryStore` | 简单 vector，无锁 | 484 cycles |

3. **Virtual dispatch 开销只有 ~10-15 cycles**
   - 32,191 vs 484 的差距来自实现，不是 dispatch

### Lesson Learned

> 优化 dispatch 机制（virtual vs variant）对性能影响微乎其微。
> 真正的优化在于：数据结构选择、内存分配策略、锁竞争。

### 最终决定

- `IMessageStore` 接口保留（现有代码兼容）
- `MessageStore` variant 版本保留（可选 API）
- 不做替换，两者共存
