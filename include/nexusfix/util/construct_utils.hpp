/*
    NexusFIX Construction Utilities

    Modern C++23 object construction using std::construct_at and std::destroy_at.

    Features:
    - Type-safe in-place construction (replaces placement new)
    - Explicit destruction (replaces manual destructor calls)
    - constexpr-capable construction for compile-time use
    - Array construction/destruction utilities

    Reference: Modern C++ Technique #13 (Placement new / std::construct_at)

    Benefits over placement new:
    - Type-safe: Returns typed pointer, no reinterpret_cast needed
    - constexpr: Can be used in constant expressions (C++20+)
    - Cleaner: More readable, intent is clearer
    - Consistent: Pairs naturally with std::destroy_at
*/

#pragma once

#include <memory>
#include <new>
#include <type_traits>
#include <cstddef>

namespace nfx::util {

// ============================================================================
// Construction Utilities
// ============================================================================

/// Construct object at given location (replaces placement new)
/// @tparam T Type to construct
/// @tparam Args Constructor argument types
/// @param ptr Location to construct at (must be properly aligned)
/// @param args Constructor arguments
/// @return Pointer to constructed object
template<typename T, typename... Args>
[[nodiscard]] constexpr T* construct(T* ptr, Args&&... args)
    noexcept(std::is_nothrow_constructible_v<T, Args...>) {
    return std::construct_at(ptr, std::forward<Args>(args)...);
}

/// Default-construct object at given location
template<typename T>
[[nodiscard]] constexpr T* construct_default(T* ptr)
    noexcept(std::is_nothrow_default_constructible_v<T>) {
    return std::construct_at(ptr);
}

/// Destroy object at given location (replaces explicit destructor call)
template<typename T>
constexpr void destroy(T* ptr) noexcept {
    std::destroy_at(ptr);
}

// ============================================================================
// Combined Destroy + Construct (for object recycling)
// ============================================================================

/// Destroy existing object and reconstruct with new arguments
/// Use for pool object recycling
template<typename T, typename... Args>
[[nodiscard]] constexpr T* reconstruct(T* ptr, Args&&... args)
    noexcept(std::is_nothrow_destructible_v<T> &&
             std::is_nothrow_constructible_v<T, Args...>) {
    std::destroy_at(ptr);
    return std::construct_at(ptr, std::forward<Args>(args)...);
}

/// Destroy and default-reconstruct (for pool reset)
template<typename T>
[[nodiscard]] constexpr T* reconstruct_default(T* ptr)
    noexcept(std::is_nothrow_destructible_v<T> &&
             std::is_nothrow_default_constructible_v<T>) {
    std::destroy_at(ptr);
    return std::construct_at(ptr);
}

// ============================================================================
// Raw Memory Construction
// ============================================================================

/// Construct object in raw memory (from allocator)
/// @param raw Raw memory pointer (void* from allocator)
/// @param args Constructor arguments
/// @return Pointer to constructed object
template<typename T, typename... Args>
[[nodiscard]] T* construct_in_raw(void* raw, Args&&... args)
    noexcept(std::is_nothrow_constructible_v<T, Args...>) {
    return std::construct_at(static_cast<T*>(raw), std::forward<Args>(args)...);
}

/// Destroy object and return raw memory pointer
template<typename T>
[[nodiscard]] void* destroy_to_raw(T* ptr) noexcept {
    std::destroy_at(ptr);
    return static_cast<void*>(ptr);
}

// ============================================================================
// Array Construction
// ============================================================================

/// Construct array of default-initialized objects
template<typename T>
constexpr void construct_array(T* first, T* last)
    noexcept(std::is_nothrow_default_constructible_v<T>) {
    for (T* p = first; p != last; ++p) {
        std::construct_at(p);
    }
}

/// Construct array with count
template<typename T>
constexpr void construct_array_n(T* first, size_t count)
    noexcept(std::is_nothrow_default_constructible_v<T>) {
    for (size_t i = 0; i < count; ++i) {
        std::construct_at(first + i);
    }
}

/// Destroy array of objects
template<typename T>
constexpr void destroy_array(T* first, T* last) noexcept {
    // Destroy in reverse order (C++ destruction order)
    while (first != last) {
        --last;
        std::destroy_at(last);
    }
}

/// Destroy array with count
template<typename T>
constexpr void destroy_array_n(T* first, size_t count) noexcept {
    // Destroy in reverse order
    for (size_t i = count; i > 0; --i) {
        std::destroy_at(first + i - 1);
    }
}

// ============================================================================
// Uninitialized Memory Operations
// ============================================================================

/// Move-construct into uninitialized memory
template<typename T>
[[nodiscard]] constexpr T* construct_move(T* dest, T& source)
    noexcept(std::is_nothrow_move_constructible_v<T>) {
    return std::construct_at(dest, std::move(source));
}

/// Copy-construct into uninitialized memory
template<typename T>
[[nodiscard]] constexpr T* construct_copy(T* dest, const T& source)
    noexcept(std::is_nothrow_copy_constructible_v<T>) {
    return std::construct_at(dest, source);
}

// ============================================================================
// Type Traits for Construction
// ============================================================================

/// Check if T can be trivially reconstructed (destroy + construct is no-op)
template<typename T>
inline constexpr bool is_trivially_reconstructible_v =
    std::is_trivially_destructible_v<T> &&
    std::is_trivially_default_constructible_v<T>;

/// Check if T can be safely pooled (trivial destruction)
template<typename T>
inline constexpr bool is_pool_safe_v =
    std::is_trivially_destructible_v<T> ||
    std::is_nothrow_destructible_v<T>;

// ============================================================================
// Pool Object RAII Wrapper
// ============================================================================

/// RAII wrapper for objects allocated from raw memory
/// Ensures proper destruction when scope exits
template<typename T>
class ConstructedObject {
public:
    /// Construct object in raw memory
    template<typename... Args>
    explicit ConstructedObject(void* raw, Args&&... args)
        : ptr_(construct_in_raw<T>(raw, std::forward<Args>(args)...)) {}

    ~ConstructedObject() {
        if (ptr_) {
            std::destroy_at(ptr_);
        }
    }

    // Move-only
    ConstructedObject(ConstructedObject&& other) noexcept
        : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }

    ConstructedObject& operator=(ConstructedObject&& other) noexcept {
        if (this != &other) {
            if (ptr_) {
                std::destroy_at(ptr_);
            }
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    ConstructedObject(const ConstructedObject&) = delete;
    ConstructedObject& operator=(const ConstructedObject&) = delete;

    /// Release ownership (caller takes responsibility for destruction)
    [[nodiscard]] T* release() noexcept {
        T* p = ptr_;
        ptr_ = nullptr;
        return p;
    }

    [[nodiscard]] T* get() noexcept { return ptr_; }
    [[nodiscard]] const T* get() const noexcept { return ptr_; }
    [[nodiscard]] T& operator*() noexcept { return *ptr_; }
    [[nodiscard]] const T& operator*() const noexcept { return *ptr_; }
    [[nodiscard]] T* operator->() noexcept { return ptr_; }
    [[nodiscard]] const T* operator->() const noexcept { return ptr_; }
    [[nodiscard]] explicit operator bool() const noexcept { return ptr_ != nullptr; }

private:
    T* ptr_;
};

} // namespace nfx::util
