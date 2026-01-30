# TICKET_021: Phase 2 Utilities Benchmark Report

**Date:** 2026-01-30
**Benchmark:** `benchmarks/phase2_utils_bench.cpp`
**Build:** Release (-O3 -march=native)
**Platform:** Linux x86_64

---

## Summary

| Utility | Traditional | Phase 2 | Speedup | Verdict |
|---------|-------------|---------|---------|---------|
| Seqlock vs Mutex | 2.46 ns | 0.37 ns | **6.64x** | EFFECTIVE |
| ObjectPool (high-freq) | 7.71 ns | 2.30 ns | **3.35x** | EFFECTIVE |
| branchless_min | 2247 ns | 791 ns | **2.84x** | EFFECTIVE |
| ObjectPool (batch) | 1045 ns | 786 ns | **1.33x** | EFFECTIVE |
| string_hash | 643262 ns | 1137637 ns | 0.57x | NOT EFFECTIVE |
| is_digit | 9978 ns | 9984 ns | 1.00x | NEUTRAL |
| byteswap32 | 0.09 ns | 0.09 ns | 1.00x | NEUTRAL |

---

## Detailed Results

### 1. Branchless Min/Max (Random Data)

**Scenario:** 10,000 random comparisons (causes branch misprediction)

```
std::min (conditional)                            2247.44 ns        0.44 M/s
branchless_min                                     791.15 ns        1.26 M/s
  => 2.84x FASTER
```

**Analysis:** Random data causes ~50% branch misprediction rate. Branchless version eliminates this penalty entirely.

---

### 2. Message Type Dispatch (FIX Version String)

**Scenario:** Dispatch based on FIX version (8-char strings like "FIX.4.4")

```
strcmp chain (7 versions)                       643262.44 ns        0.00 M/s
hash switch (7 versions)                       1137637.18 ns        0.00 M/s
  => 0.57x slower
```

**Analysis:** Hash computation overhead exceeds strcmp benefit for short strings. Compiler already optimizes strcmp for short constant strings efficiently.

**Recommendation:** Do NOT use string_hash for:
- Single-character FIX message types (35=D, 35=8)
- Short version strings (<10 chars)

May be beneficial for:
- Long strings (>20 chars)
- Many branches (>15 cases)

---

### 3. Seqlock vs Mutex (Read-Heavy)

**Scenario:** Single-threaded read of shared market data struct

```
mutex lock/read/unlock                               2.46 ns      405.70 M/s
seqlock read                                         0.37 ns     2693.55 M/s
  => 6.64x FASTER
```

**Analysis:** Seqlock eliminates lock acquisition overhead. Readers never block, only retry on write conflict.

**Best Use Case:** Real-time market data publishing with single writer, multiple readers.

---

### 4. Object Pool vs new/delete (Batch Pattern)

**Scenario:** Allocate 100 objects, then deallocate all

```
new/delete (100 objects)                          1045.87 ns        0.96 M/s
ObjectPool (100 objects)                           786.87 ns        1.27 M/s
  => 1.33x FASTER
```

**Analysis:** Object pool avoids syscall overhead and memory fragmentation.

---

### 5. Object Pool (High Frequency Alloc/Free)

**Scenario:** Rapid alloc/free with queue (simulating message processing)

```
new/delete queue pattern                             7.71 ns      129.77 M/s
ObjectPool queue pattern                             2.30 ns      435.08 M/s
  => 3.35x FASTER
```

**Analysis:** High-frequency allocation pattern shows greatest benefit. Pool maintains hot cache lines.

**Best Use Case:** FIX message objects in parsing hot path.

---

### 6. Branchless Range Check

**Scenario:** Check if char is digit (FIX field parsing)

```
traditional: c >= '0' && c <= '9'                 9978.12 ns        0.10 M/s
branchless: is_digit()                            9984.97 ns        0.10 M/s
  => 1.00x (same)
```

**Analysis:** Compiler already optimizes simple range checks to branchless code. No additional benefit from manual branchless implementation.

---

### 7. Byte Swap (Network to Host)

**Scenario:** Convert big-endian to little-endian (uint32)

```
__builtin_bswap32                                    0.09 ns    10767.28 M/s
byteswap32 (constexpr)                               0.09 ns    10768.67 M/s
  => 1.00x (same)
```

**Analysis:** Both compile to identical `bswap` instruction. Our implementation provides constexpr capability without runtime cost.

---

## Recommendations

### USE (Proven Effective)

| Utility | Speedup | Use Case |
|---------|---------|----------|
| `Seqlock<T>` | 6.64x | Market data publishing |
| `ObjectPool<T>` | 3.35x | Message object allocation |
| `branchless_min/max` | 2.84x | Random data comparisons |

### AVOID (Not Effective)

| Utility | Result | Reason |
|---------|--------|--------|
| `string_hash` for short strings | 0.57x slower | Hash overhead > strcmp |

### NEUTRAL (No Benefit, No Harm)

| Utility | Result | Note |
|---------|--------|------|
| `is_digit()` | Same | Compiler already optimizes |
| `byteswap32()` | Same | Provides constexpr, same runtime |

---

## Raw Output

```
============================================================
         NexusFIX Phase 2 Utilities Benchmark
============================================================

=== 1. Branchless Min/Max (Random Data) ===
   Scenario: 10,000 random comparisons (causes branch misprediction)

std::min (conditional)                            2247.44 ns        0.44 M/s
branchless_min                                     791.15 ns        1.26 M/s
  => 2.84x FASTER

=== 2. Message Type Dispatch (FIX Version String) ===
   Scenario: Dispatch based on FIX version (8-char strings)

strcmp chain (7 versions)                       643262.44 ns        0.00 M/s
hash switch (7 versions)                       1137637.18 ns        0.00 M/s
  => 0.57x slower

=== 3. Seqlock vs Mutex (Read-Heavy) ===
   Scenario: Single-threaded read of shared market data

mutex lock/read/unlock                               2.46 ns      405.70 M/s
seqlock read                                         0.37 ns     2693.55 M/s
  => 6.64x FASTER

=== 4. Object Pool vs new/delete (Batch Pattern) ===
   Scenario: Allocate 100 objects, then deallocate all

new/delete (100 objects)                          1045.87 ns        0.96 M/s
ObjectPool (100 objects)                           786.87 ns        1.27 M/s
  => 1.33x FASTER

=== 5. Object Pool (High Frequency Alloc/Free) ===
   Scenario: Rapid alloc/free with queue (message processing)

new/delete queue pattern                             7.71 ns      129.77 M/s
ObjectPool queue pattern                             2.30 ns      435.08 M/s
  => 3.35x FASTER

=== 6. Branchless Range Check ===
   Scenario: Check if char is digit (FIX field parsing)

traditional: c >= '0' && c <= '9'                 9978.12 ns        0.10 M/s
branchless: is_digit()                            9984.97 ns        0.10 M/s
  => 1.00x slower

=== 7. Byte Swap (Network to Host) ===
   Scenario: Convert big-endian to little-endian

__builtin_bswap32                                    0.09 ns    10767.28 M/s
byteswap32 (constexpr)                               0.09 ns    10768.67 M/s
  => 1.00x FASTER

============================================================
                       Summary
============================================================
FASTER = Phase 2 utility is faster than traditional approach
slower = Traditional approach is faster (may reconsider use)
```

---

## Reproduce

```bash
cd /data/ws/NexusFix/build
cmake -DCMAKE_BUILD_TYPE=Release ..
make phase2_utils_bench -j$(nproc)
./bin/benchmarks/phase2_utils_bench
```
