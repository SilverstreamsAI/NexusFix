# Contributing to NexusFIX

## Current Policy

**Pull requests are limited to bug fixes only.** Feature PRs will not be accepted at this time.

Issues and discussions are welcome for bug reports, performance questions, and feature ideas.

## Bug Fix PRs

To submit a bug fix:

1. Open an issue describing the bug with reproduction steps
2. Fork the repository and create a branch from `main`
3. Fix the bug and add a test case that reproduces it
4. Ensure all existing tests pass
5. Submit a PR referencing the issue

## Building from Source

### Requirements

- C++23 compiler: GCC 13+ or Clang 17+
- CMake 3.25+
- Linux, macOS, or Windows

### Build Steps

```bash
git clone https://github.com/SilverstreamsAI/NexusFIX.git
cd NexusFIX

# Release build
./start.sh build

# Debug build
./start.sh debug

# Build with tests
cmake -B build -DCMAKE_BUILD_TYPE=Release -DNFX_BUILD_TESTS=ON
cmake --build build --parallel

# Run tests
cd build && ctest --output-on-failure
```

Dependencies (Quill, Abseil, xsimd) are fetched automatically via CMake FetchContent.

## Code Standards

All contributions must follow these standards:

- **C++23** (ISO/IEC 14882:2024)
- **Zero allocation on hot paths** (use PMR pools or stack allocation)
- **`noexcept`** on all hot path functions
- **`[[nodiscard]]`** on all APIs that return values
- **No `new`/`delete`** on hot paths
- **No `std::endl`** (use `\n`)
- **No exceptions** on hot paths (use `std::expected`)
- **No `virtual`** in performance-critical code

Performance-sensitive changes must include benchmark results showing no regression.

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
