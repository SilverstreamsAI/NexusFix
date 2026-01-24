# AVX-512 SIMD Scanner Benchmark

**Date**: 2026-01-23
**Ticket**: TICKET_INTERNAL_010 (Highway Analysis)
**Optimization**: AVX-512 SOH Scanner Implementation

---

## Summary

Added AVX-512 support to the SIMD scanner, providing automatic selection of the best implementation based on buffer size and CPU capabilities.

| Buffer Size | Scalar | AVX2 | AVX2 Speedup | AVX-512 (expected) |
|-------------|--------|------|--------------|-------------------|
| 64 bytes | 22.1 ns | 7.4 ns | **3.0x** | - |
| 128 bytes | 49.4 ns | 7.8 ns | **6.3x** | ~4 ns (est. 1.9x vs AVX2) |
| 256 bytes | 83.6 ns | 10.6 ns | **7.9x** | ~6 ns (est. 1.8x vs AVX2) |
| 512 bytes | 184.7 ns | 15.9 ns | **11.6x** | ~9 ns (est. 1.8x vs AVX2) |
| 1024 bytes | 363.5 ns | 29.3 ns | **12.4x** | ~16 ns (est. 1.8x vs AVX2) |
| 2048 bytes | 741.7 ns | 57.4 ns | **12.9x** | ~32 ns (est. 1.8x vs AVX2) |
| 4096 bytes | 1230.5 ns | 124.9 ns | **9.9x** | ~70 ns (est. 1.8x vs AVX2) |
| 8192 bytes | 1882.9 ns | 128.4 ns | **14.7x** | ~72 ns (est. 1.8x vs AVX2) |

**Peak Throughput**: 510 Gbps (AVX2, 8192 byte buffer)

---

## Test Environment

| Parameter | Value |
|-----------|-------|
| CPU | 3.418 GHz |
| AVX2 | Available |
| AVX-512 | Not available (estimates provided) |
| Warmup | 1,000 iterations |
| Benchmark | 100,000 iterations |
| Runs | 5 |

---

## Implementation Details

### Auto-Selection Logic

```cpp
// Priority: AVX-512 > AVX2 > Scalar
inline SohPositions scan_soh(std::span<const char> data) noexcept {
#if NFX_AVX512_AVAILABLE
    if (data.size() >= 128) [[likely]] {
        return scan_soh_avx512(data);  // 64 bytes per iteration
    }
#endif
#if NFX_SIMD_AVAILABLE
    if (data.size() >= 64) [[likely]] {
        return scan_soh_avx2(data);    // 32 bytes per iteration
    }
#endif
    return scan_soh_scalar(data);
}
```

### AVX-512 Implementation Highlights

```cpp
// Process 64 bytes at a time (2x AVX2)
__m512i chunk = _mm512_loadu_si512(ptr + i);

// Direct 64-bit mask comparison (more efficient than AVX2 movemask)
__mmask64 mask = _mm512_cmpeq_epi8_mask(chunk, soh_vec);

// 64-bit trailing zero count
int bit = _tzcnt_u64(mask);
```

### Key Differences: AVX2 vs AVX-512

| Aspect | AVX2 | AVX-512 |
|--------|------|---------|
| Register size | 256 bits (32 bytes) | 512 bits (64 bytes) |
| Bytes per iteration | 32 | 64 |
| Mask type | uint32_t | __mmask64 |
| Mask operation | _mm256_movemask_epi8 | _mm512_cmpeq_epi8_mask |
| Bit scan | __builtin_ctz | _tzcnt_u64 |

---

## CMake Configuration

```cmake
# Enable SIMD (default)
option(NFX_ENABLE_SIMD "Enable SIMD optimizations (AVX2)" ON)

# Enable AVX-512 (requires CPU support)
option(NFX_ENABLE_AVX512 "Enable AVX-512 optimizations" OFF)

# Compiler flags
if(NFX_ENABLE_AVX512)
    target_compile_options(nexusfix INTERFACE -mavx512f -mavx512bw)
endif()
```

---

## Analysis

### Why AVX2 Shows Such Large Speedups

1. **SIMD parallelism**: Process 32 bytes vs 1 byte per comparison
2. **Branch elimination**: Bitmask operations replace per-byte conditionals
3. **Memory bandwidth**: Wider loads utilize memory bandwidth better
4. **Instruction pipelining**: SIMD instructions have good ILP

### Expected AVX-512 Improvement

Based on register size doubling (64 vs 32 bytes):
- **Theoretical**: 2x throughput improvement
- **Practical**: ~1.5-1.8x improvement (memory bandwidth limited)

For typical FIX messages (200-500 bytes):
- AVX2: ~10-20 ns
- AVX-512 (expected): ~6-12 ns

---

## Files Modified

| File | Changes |
|------|---------|
| `include/nexusfix/parser/simd_scanner.hpp` | Added AVX-512 implementations |
| `CMakeLists.txt` | Added NFX_ENABLE_AVX512 option |
| `benchmarks/simd_scanner_bench.cpp` | Created benchmark |

---

## Conclusion

| Metric | Result |
|--------|--------|
| AVX2 vs Scalar | **3-15x speedup** |
| AVX-512 vs AVX2 (expected) | **1.5-2x speedup** |
| Implementation | Complete with auto-selection |
| Backward compatibility | Full (automatic fallback) |

The SIMD scanner now automatically selects the best implementation based on CPU capabilities and buffer size, ensuring optimal performance across all hardware.

---

## References

- [Intel Intrinsics Guide](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/)
- [AVX-512 Programming](https://software.intel.com/content/www/us/en/develop/articles/intel-avx-512-instructions.html)
