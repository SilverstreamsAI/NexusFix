# Changelog

All notable changes to NexusFIX will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.15] - 2026-02-07

### Changed
- Replaced raw `<immintrin.h>` intrinsics with xsimd 13.2.0 portable SIMD abstraction
- Arch-templated SIMD functions: `scan_soh_xsimd<Arch>()`, `find_soh_xsimd<Arch>()`, `find_equals_xsimd<Arch>()`, `count_soh_xsimd<Arch>()`, `checksum_xsimd<Arch>()`, `build_index_xsimd<Arch>()`
- ARM NEON ready via arch-templated functions

### Added
- CMake option `NFX_ENABLE_XSIMD` (default ON)
- Raw intrinsics retained as fallback when `NFX_ENABLE_XSIMD=OFF`

## [0.1.14] - 2026-02-06

### Added
- mimalloc per-session heap integration (TICKET_210)
- Per-session `mi_heap_t*` for O(1) bulk cleanup on session teardown

### Fixed
- Added CTest `Testing/` directory to `.gitignore`

## [0.1.13] - 2026-02-05

### Changed
- 77% faster ExecutionReport parsing (207 ns to 47 ns P50) via simdjson-style two-stage structural indexing (TICKET_208)
- Two-stage SIMD pipeline: structural index + lazy O(1) field extraction
- Runtime dispatch: AVX-512 / AVX2 / scalar fallback
- Throughput up to 3.32 GB/s (ExecutionReport, 169 bytes)

### Fixed
- Windows MSVC CI: suppressed C4324 alignment warning and replaced deprecated `std::getenv()`

## [0.1.12] - 2026-02-03

### Added
- C++23 I/O and Ranges modernization (TICKET_024 Phase 3/5)
- `print()`/`println()`, `enumerate()`, `chunk()`, `slide()`, `stride()`, `contains()` utilities
- `format_to_string()` / `build_string()` zero-init buffer building
- `if consteval` replaces `is_constant_evaluated()`
- `uz` size_t literals for type safety

### Changed
- 45% overall C++23 adoption rate (28 features adopted)
- Zero performance impact (pure code modernization)

## [0.1.11] - 2026-02-02

### Changed
- P99 latency 8.6% faster (227 ns to 207 ns), P99.9 29.8% faster (365 ns to 256 ns)
- `NFX_ASSUME()` cross-platform optimization hints
- `std::unreachable()`, `std::to_underlying()`, `std::string::contains()` adoption

### Fixed
- Apple Clang `-Wassume` warning compatibility
- Clang 18 `[[assume]]` version detection

### Added
- Compiler support matrix: GCC 13+, Clang 19+, Apple Clang, MSVC 19.29+
- Benchmark reports for TICKET_023, TICKET_200, TICKET_202, TICKET_204

## [0.1.10] - 2026-02-01

### Changed
- 22 compile-time lookup tables, ~300 runtime branches eliminated (TICKET_023)
- 55-92% faster enum-to-string conversions across all components
- `md_entry_type_name()` 92% faster, `detect_version()` 97% faster
- ~25 switch statements replaced with O(1) lookups
- API fully backward compatible

## [0.1.9] - 2026-02-01

### Added
- Compile-time MsgType dispatch table (TICKET_022)
- 16 `static_assert` compile-time validations

### Changed
- 73% faster message type lookup via O(1) array replacing 17-case switch
- `name()` 73.4% faster, `is_admin()` 49.8% faster, random access 78.6% faster

## [0.1.8] - 2026-01-30

### Added
- Seqlock: 6.64x faster market data publishing (TICKET_021)
- ObjectPool: 3.35x faster message object allocation
- `branchless_min`: 2.84x faster branch-free comparisons
- Bit utilities with `std::bit_cast` and byte parsing
- FNV-1a compile-time string hashing
- `mlockall` wrapper for memory locking

## [0.1.7] - 2026-01-30

### Added
- Simple Binary Encoding (SBE) for internal IPC (TICKET_203)
- Zero-copy flyweight decode
- SBE Decode 35.12 ns, SBE Encode 13.17 ns (5.5x faster than FIX text)

## [0.1.6] - 2026-01-30

### Added
- Lock-free Multi-Producer Single-Consumer queue with Disruptor pattern (TICKET_202)
- Pluggable wait strategies: BusySpinWait, YieldingWait, SleepingWait, BackoffWait
- Batch pop operations for bulk consumption
- False-sharing elimination via cache-line padding

### Changed
- MPSC throughput: 32.84 M/s with only 5.5% overhead vs SPSC

## [0.1.5] - 2026-01-29

### Changed
- Modernized object construction with `std::construct_at` / `std::destroy_at`
- Type-safe memory pool operations (replaced `void*` casts with `T*` typed)
- constexpr support for construction

## [0.1.4] - 2026-01-29

### Added
- Compile-time diagnostics (static assertions, layout validation)
- Zero-allocation formatting utilities
- I-Cache warming for consistent latency
- C++23 ranges utilities for zero-copy operations

### Changed
- SIMD scanner: improved platform detection for AVX2/SSE4.2 fallback
- Repeating group parser enhancements

### Fixed
- Market data test case fixes

## [0.1.2] - 2026-01-28

### Added
- Release workflow with automated binary packaging
- CMake install targets for system-wide installation

### Fixed
- CMake install export error with FetchContent dependencies

## [0.1.1] - 2026-01-28

### Added
- Cross-platform transport layer (Windows, macOS, Linux)
- PMR pool allocation optimization for MemoryMessageStore
- C++11 vs C++23 performance case study documentation
- Release workflow and CMake install targets

### Fixed
- Windows platform_test crash: Winsock initialization before socket calls

## [0.1.0] - 2026-01-25

### Added
- Initial release of NexusFIX high-performance FIX protocol engine
- 3x faster than QuickFIX (~250 ns vs ~730 ns ExecutionReport parse)
- Zero heap allocation on hot path
- SIMD-accelerated parsing (AVX2/AVX-512)
- io_uring transport support (Linux)
- FIX 4.4 and FIX 5.0 + FIXT 1.1 support
- Session messages: Logon, Logout, Heartbeat, TestRequest, ResendRequest
- Order messages: NewOrderSingle, OrderCancelRequest, ExecutionReport
- Market Data messages: MarketDataRequest, Snapshot, Incremental

[0.1.15]: https://github.com/SilverstreamsAI/NexusFix/compare/v0.1.14...v0.1.15
[0.1.14]: https://github.com/SilverstreamsAI/NexusFix/compare/v0.1.13...v0.1.14
[0.1.13]: https://github.com/SilverstreamsAI/NexusFix/compare/v0.1.12...v0.1.13
[0.1.12]: https://github.com/SilverstreamsAI/NexusFix/compare/v0.1.11...v0.1.12
[0.1.11]: https://github.com/SilverstreamsAI/NexusFix/compare/v0.1.10...v0.1.11
[0.1.10]: https://github.com/SilverstreamsAI/NexusFix/compare/v0.1.9...v0.1.10
[0.1.9]: https://github.com/SilverstreamsAI/NexusFix/compare/v0.1.8...v0.1.9
[0.1.8]: https://github.com/SilverstreamsAI/NexusFix/compare/v0.1.7...v0.1.8
[0.1.7]: https://github.com/SilverstreamsAI/NexusFix/compare/v0.1.6...v0.1.7
[0.1.6]: https://github.com/SilverstreamsAI/NexusFix/compare/v0.1.5...v0.1.6
[0.1.5]: https://github.com/SilverstreamsAI/NexusFix/compare/v0.1.4...v0.1.5
[0.1.4]: https://github.com/SilverstreamsAI/NexusFix/compare/v0.1.2...v0.1.4
[0.1.2]: https://github.com/SilverstreamsAI/NexusFix/compare/v0.1.1...v0.1.2
[0.1.1]: https://github.com/SilverstreamsAI/NexusFix/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/SilverstreamsAI/NexusFix/releases/tag/v0.1.0
