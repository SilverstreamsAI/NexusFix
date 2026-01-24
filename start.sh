#!/bin/bash
# start.sh - NexusFIX Build and Run Script
# Usage: ./start.sh [command]
#   clean     - Clean build directory
#   build     - Build project (default: Release)
#   debug     - Build with Debug configuration
#   test      - Run tests
#   bench     - Run benchmarks
#   all       - Clean, build, test, bench
#   help      - Show this help

set -e

# Project root directory
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
BIN_DIR="${BUILD_DIR}/bin"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Print colored message
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Print header
print_header() {
    echo ""
    echo "============================================"
    echo "  NexusFIX - High Performance FIX Engine"
    echo "============================================"
    echo ""
}

# Clean build directory
do_clean() {
    log_info "Cleaning build directory..."
    if [ -d "${BUILD_DIR}" ]; then
        rm -rf "${BUILD_DIR}"
        log_success "Build directory cleaned"
    else
        log_info "Build directory does not exist, nothing to clean"
    fi
}

# Configure and build
do_build() {
    local build_type="${1:-Release}"

    log_info "Building NexusFIX (${build_type})..."

    # Create build directory
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"

    # Configure with CMake
    log_info "Configuring with CMake..."
    cmake .. \
        -DCMAKE_BUILD_TYPE="${build_type}" \
        -DNFX_ENABLE_SIMD=ON \
        -DNFX_ENABLE_IO_URING=OFF \
        -DNFX_BUILD_BENCHMARKS=ON \
        -DNFX_BUILD_TESTS=OFF \
        -DNFX_BUILD_EXAMPLES=ON

    # Build
    log_info "Compiling..."
    local num_cores=$(nproc 2>/dev/null || echo 4)
    make -j${num_cores}

    log_success "Build completed successfully"

    # List built artifacts
    echo ""
    log_info "Built artifacts:"
    if [ -d "${BIN_DIR}/benchmarks" ]; then
        echo "  Benchmarks:"
        ls -1 "${BIN_DIR}/benchmarks" 2>/dev/null | sed 's/^/    - /'
    fi
    if [ -d "${BIN_DIR}/examples" ]; then
        echo "  Examples:"
        ls -1 "${BIN_DIR}/examples" 2>/dev/null | sed 's/^/    - /'
    fi
    if [ -d "${BIN_DIR}/tests" ]; then
        echo "  Tests:"
        ls -1 "${BIN_DIR}/tests" 2>/dev/null | sed 's/^/    - /'
    fi
}

# Build with tests enabled (requires Catch2)
do_build_with_tests() {
    local build_type="${1:-Release}"

    log_info "Building NexusFIX with tests (${build_type})..."

    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"

    cmake .. \
        -DCMAKE_BUILD_TYPE="${build_type}" \
        -DNFX_ENABLE_SIMD=ON \
        -DNFX_ENABLE_IO_URING=OFF \
        -DNFX_BUILD_BENCHMARKS=ON \
        -DNFX_BUILD_TESTS=ON \
        -DNFX_BUILD_EXAMPLES=ON

    local num_cores=$(nproc 2>/dev/null || echo 4)
    make -j${num_cores}

    log_success "Build completed successfully"
}

# Run tests
do_test() {
    log_info "Running tests..."

    if [ ! -d "${BUILD_DIR}" ]; then
        log_warn "Build directory not found, building first..."
        do_build_with_tests
    fi

    cd "${BUILD_DIR}"

    # Run QuickFIX compatibility tests
    if [ -x "${BIN_DIR}/tests/quickfix_compat" ]; then
        echo ""
        log_info "Running QuickFIX compatibility tests..."
        "${BIN_DIR}/tests/quickfix_compat"
    else
        log_warn "quickfix_compat not found, skipping"
    fi

    # Run Catch2 tests if available
    if [ -x "${BIN_DIR}/tests/nexusfix_tests" ]; then
        echo ""
        log_info "Running unit tests..."
        "${BIN_DIR}/tests/nexusfix_tests"
    fi

    # Run CTest if available
    if [ -f "${BUILD_DIR}/CTestTestfile.cmake" ]; then
        echo ""
        log_info "Running CTest..."
        ctest --output-on-failure
    fi

    log_success "Tests completed"
}

# Run benchmarks
do_bench() {
    local iterations="${1:-100000}"

    log_info "Running benchmarks (${iterations} iterations)..."

    if [ ! -d "${BUILD_DIR}" ]; then
        log_warn "Build directory not found, building first..."
        do_build "Release"
    fi

    # Run parse benchmark
    if [ -x "${BIN_DIR}/benchmarks/parse_benchmark" ]; then
        echo ""
        log_info "Running parse benchmark..."
        "${BIN_DIR}/benchmarks/parse_benchmark" "${iterations}"
    else
        log_error "parse_benchmark not found"
    fi

    # Run session benchmark
    if [ -x "${BIN_DIR}/benchmarks/session_benchmark" ]; then
        echo ""
        log_info "Running session benchmark..."
        "${BIN_DIR}/benchmarks/session_benchmark" "${iterations}"
    else
        log_error "session_benchmark not found"
    fi

    log_success "Benchmarks completed"
}

# Run QuickFIX comparison benchmark
do_compare() {
    local iterations="${1:-100000}"

    log_info "Running QuickFIX comparison benchmark (${iterations} iterations)..."

    if [ ! -d "${BUILD_DIR}" ]; then
        log_warn "Build directory not found, building first..."
        do_build "Release"
    fi

    # Run QuickFIX comparison benchmark
    if [ -x "${BIN_DIR}/benchmarks/quickfix_benchmark" ]; then
        echo ""
        "${BIN_DIR}/benchmarks/quickfix_benchmark" "${iterations}"
    else
        log_error "quickfix_benchmark not found"
        log_info "Make sure libquickfix-dev is installed: sudo apt-get install libquickfix-dev"
    fi

    log_success "Comparison benchmark completed"
}

# Run example client
do_run_client() {
    local host="${1:-localhost}"
    local port="${2:-9876}"

    log_info "Running simple client (${host}:${port})..."

    if [ ! -x "${BIN_DIR}/examples/simple_client" ]; then
        log_warn "simple_client not found, building first..."
        do_build "Release"
    fi

    "${BIN_DIR}/examples/simple_client" --host "${host}" --port "${port}"
}

# Show help
do_help() {
    echo "Usage: $0 [command] [options]"
    echo ""
    echo "Commands:"
    echo "  clean              Clean build directory"
    echo "  build              Build project (Release)"
    echo "  debug              Build project (Debug)"
    echo "  test               Run tests"
    echo "  bench [N]          Run benchmarks (default: 100000 iterations)"
    echo "  compare [N]        Run QuickFIX vs NexusFIX comparison benchmark"
    echo "  client [host port] Run example client"
    echo "  all                Clean, build, test, bench"
    echo "  help               Show this help"
    echo ""
    echo "Examples:"
    echo "  $0 clean           # Clean build"
    echo "  $0 build           # Release build"
    echo "  $0 debug           # Debug build"
    echo "  $0 bench 1000000   # Run benchmarks with 1M iterations"
    echo "  $0 all             # Full clean build and test"
    echo ""
}

# Main
main() {
    print_header

    local command="${1:-build}"
    shift || true

    case "${command}" in
        clean)
            do_clean
            ;;
        build)
            do_build "Release"
            ;;
        debug)
            do_build "Debug"
            ;;
        test)
            do_test
            ;;
        bench|benchmark)
            do_bench "$@"
            ;;
        compare)
            do_compare "$@"
            ;;
        client)
            do_run_client "$@"
            ;;
        all)
            do_clean
            do_build "Release"
            do_test
            do_bench
            ;;
        help|--help|-h)
            do_help
            ;;
        *)
            log_error "Unknown command: ${command}"
            do_help
            exit 1
            ;;
    esac
}

main "$@"
