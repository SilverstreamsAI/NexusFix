#pragma once

#include <array>
#include <string_view>
#include <cstdint>

namespace nfx {

// ============================================================================
// FIX Protocol Versions
// ============================================================================

namespace fix_version {

// FIX 4.x versions (combined session + application)
inline constexpr std::string_view FIX_4_0 = "FIX.4.0";
inline constexpr std::string_view FIX_4_1 = "FIX.4.1";
inline constexpr std::string_view FIX_4_2 = "FIX.4.2";
inline constexpr std::string_view FIX_4_3 = "FIX.4.3";
inline constexpr std::string_view FIX_4_4 = "FIX.4.4";

// FIXT 1.1 transport layer (session messages only)
inline constexpr std::string_view FIXT_1_1 = "FIXT.1.1";

// FIX 5.x application versions (used with FIXT.1.1)
inline constexpr std::string_view FIX_5_0 = "FIX.5.0";
inline constexpr std::string_view FIX_5_0_SP1 = "FIX.5.0SP1";
inline constexpr std::string_view FIX_5_0_SP2 = "FIX.5.0SP2";

} // namespace fix_version

// ============================================================================
// Application Version ID (Tag 1128)
// ============================================================================

/// ApplVerID values for FIX 5.0+
namespace appl_ver_id {
    inline constexpr char FIX_2_7 = '0';
    inline constexpr char FIX_3_0 = '1';
    inline constexpr char FIX_4_0 = '2';
    inline constexpr char FIX_4_1 = '3';
    inline constexpr char FIX_4_2 = '4';
    inline constexpr char FIX_4_3 = '5';
    inline constexpr char FIX_4_4 = '6';
    inline constexpr char FIX_5_0 = '7';
    inline constexpr char FIX_5_0_SP1 = '8';
    inline constexpr char FIX_5_0_SP2 = '9';

    // ========================================================================
    // Compile-time ApplVerID Info (TICKET_023)
    // ========================================================================

    namespace detail {

    template<char Ver>
    struct ApplVerInfo {
        static constexpr std::string_view string = "Unknown";
    };

    template<> struct ApplVerInfo<'0'> { static constexpr std::string_view string = "FIX.2.7"; };
    template<> struct ApplVerInfo<'1'> { static constexpr std::string_view string = "FIX.3.0"; };
    template<> struct ApplVerInfo<'2'> { static constexpr std::string_view string = "FIX.4.0"; };
    template<> struct ApplVerInfo<'3'> { static constexpr std::string_view string = "FIX.4.1"; };
    template<> struct ApplVerInfo<'4'> { static constexpr std::string_view string = "FIX.4.2"; };
    template<> struct ApplVerInfo<'5'> { static constexpr std::string_view string = "FIX.4.3"; };
    template<> struct ApplVerInfo<'6'> { static constexpr std::string_view string = "FIX.4.4"; };
    template<> struct ApplVerInfo<'7'> { static constexpr std::string_view string = "FIX.5.0"; };
    template<> struct ApplVerInfo<'8'> { static constexpr std::string_view string = "FIX.5.0SP1"; };
    template<> struct ApplVerInfo<'9'> { static constexpr std::string_view string = "FIX.5.0SP2"; };

    /// Generate ApplVerID lookup table (index by char '0'-'9')
    consteval std::array<std::string_view, 10> create_appl_ver_table() {
        std::array<std::string_view, 10> table{};
        table[0] = ApplVerInfo<'0'>::string;
        table[1] = ApplVerInfo<'1'>::string;
        table[2] = ApplVerInfo<'2'>::string;
        table[3] = ApplVerInfo<'3'>::string;
        table[4] = ApplVerInfo<'4'>::string;
        table[5] = ApplVerInfo<'5'>::string;
        table[6] = ApplVerInfo<'6'>::string;
        table[7] = ApplVerInfo<'7'>::string;
        table[8] = ApplVerInfo<'8'>::string;
        table[9] = ApplVerInfo<'9'>::string;
        return table;
    }

    inline constexpr auto APPL_VER_TABLE = create_appl_ver_table();

    } // namespace detail

    /// Compile-time query
    template<char Ver>
    [[nodiscard]] consteval std::string_view to_string() noexcept {
        return detail::ApplVerInfo<Ver>::string;
    }

    /// Runtime query using O(1) lookup
    [[nodiscard]] inline constexpr std::string_view to_string(char ver) noexcept {
        if (ver >= '0' && ver <= '9') [[likely]] {
            return detail::APPL_VER_TABLE[ver - '0'];
        }
        return "Unknown";
    }

    // Static assertions
    static_assert(detail::ApplVerInfo<'0'>::string == "FIX.2.7");
    static_assert(detail::ApplVerInfo<'6'>::string == "FIX.4.4");
    static_assert(detail::APPL_VER_TABLE[0] == "FIX.2.7");
    static_assert(detail::APPL_VER_TABLE[6] == "FIX.4.4");
}

// ============================================================================
// FIX Version Detection
// ============================================================================

/// FIX protocol version enumeration
enum class FixVersion : uint8_t {
    Unknown = 0,
    FIX_4_0,
    FIX_4_1,
    FIX_4_2,
    FIX_4_3,
    FIX_4_4,
    FIXT_1_1,  // Transport layer for FIX 5.0+
    FIX_5_0,
    FIX_5_0_SP1,
    FIX_5_0_SP2
};

inline constexpr size_t FIX_VERSION_COUNT = 10;

// ============================================================================
// Compile-time FixVersion Info (TICKET_023)
// ============================================================================

namespace detail {

template<FixVersion Ver>
struct VersionInfo {
    static constexpr std::string_view string = "Unknown";
    static constexpr bool is_fixt = false;
    static constexpr bool is_fix4 = false;
};

template<> struct VersionInfo<FixVersion::Unknown> {
    static constexpr std::string_view string = "Unknown";
    static constexpr bool is_fixt = false;
    static constexpr bool is_fix4 = false;
};

template<> struct VersionInfo<FixVersion::FIX_4_0> {
    static constexpr std::string_view string = "FIX.4.0";
    static constexpr bool is_fixt = false;
    static constexpr bool is_fix4 = true;
};

template<> struct VersionInfo<FixVersion::FIX_4_1> {
    static constexpr std::string_view string = "FIX.4.1";
    static constexpr bool is_fixt = false;
    static constexpr bool is_fix4 = true;
};

template<> struct VersionInfo<FixVersion::FIX_4_2> {
    static constexpr std::string_view string = "FIX.4.2";
    static constexpr bool is_fixt = false;
    static constexpr bool is_fix4 = true;
};

template<> struct VersionInfo<FixVersion::FIX_4_3> {
    static constexpr std::string_view string = "FIX.4.3";
    static constexpr bool is_fixt = false;
    static constexpr bool is_fix4 = true;
};

template<> struct VersionInfo<FixVersion::FIX_4_4> {
    static constexpr std::string_view string = "FIX.4.4";
    static constexpr bool is_fixt = false;
    static constexpr bool is_fix4 = true;
};

template<> struct VersionInfo<FixVersion::FIXT_1_1> {
    static constexpr std::string_view string = "FIXT.1.1";
    static constexpr bool is_fixt = true;
    static constexpr bool is_fix4 = false;
};

template<> struct VersionInfo<FixVersion::FIX_5_0> {
    static constexpr std::string_view string = "FIX.5.0";
    static constexpr bool is_fixt = true;
    static constexpr bool is_fix4 = false;
};

template<> struct VersionInfo<FixVersion::FIX_5_0_SP1> {
    static constexpr std::string_view string = "FIX.5.0SP1";
    static constexpr bool is_fixt = true;
    static constexpr bool is_fix4 = false;
};

template<> struct VersionInfo<FixVersion::FIX_5_0_SP2> {
    static constexpr std::string_view string = "FIX.5.0SP2";
    static constexpr bool is_fixt = true;
    static constexpr bool is_fix4 = false;
};

struct VersionEntry {
    std::string_view string;
    bool is_fixt;
    bool is_fix4;
};

/// Generate version lookup table
consteval std::array<VersionEntry, FIX_VERSION_COUNT> create_version_table() {
    std::array<VersionEntry, FIX_VERSION_COUNT> table{};
    table[0] = {VersionInfo<FixVersion::Unknown>::string,
                VersionInfo<FixVersion::Unknown>::is_fixt,
                VersionInfo<FixVersion::Unknown>::is_fix4};
    table[1] = {VersionInfo<FixVersion::FIX_4_0>::string,
                VersionInfo<FixVersion::FIX_4_0>::is_fixt,
                VersionInfo<FixVersion::FIX_4_0>::is_fix4};
    table[2] = {VersionInfo<FixVersion::FIX_4_1>::string,
                VersionInfo<FixVersion::FIX_4_1>::is_fixt,
                VersionInfo<FixVersion::FIX_4_1>::is_fix4};
    table[3] = {VersionInfo<FixVersion::FIX_4_2>::string,
                VersionInfo<FixVersion::FIX_4_2>::is_fixt,
                VersionInfo<FixVersion::FIX_4_2>::is_fix4};
    table[4] = {VersionInfo<FixVersion::FIX_4_3>::string,
                VersionInfo<FixVersion::FIX_4_3>::is_fixt,
                VersionInfo<FixVersion::FIX_4_3>::is_fix4};
    table[5] = {VersionInfo<FixVersion::FIX_4_4>::string,
                VersionInfo<FixVersion::FIX_4_4>::is_fixt,
                VersionInfo<FixVersion::FIX_4_4>::is_fix4};
    table[6] = {VersionInfo<FixVersion::FIXT_1_1>::string,
                VersionInfo<FixVersion::FIXT_1_1>::is_fixt,
                VersionInfo<FixVersion::FIXT_1_1>::is_fix4};
    table[7] = {VersionInfo<FixVersion::FIX_5_0>::string,
                VersionInfo<FixVersion::FIX_5_0>::is_fixt,
                VersionInfo<FixVersion::FIX_5_0>::is_fix4};
    table[8] = {VersionInfo<FixVersion::FIX_5_0_SP1>::string,
                VersionInfo<FixVersion::FIX_5_0_SP1>::is_fixt,
                VersionInfo<FixVersion::FIX_5_0_SP1>::is_fix4};
    table[9] = {VersionInfo<FixVersion::FIX_5_0_SP2>::string,
                VersionInfo<FixVersion::FIX_5_0_SP2>::is_fixt,
                VersionInfo<FixVersion::FIX_5_0_SP2>::is_fix4};
    return table;
}

inline constexpr auto VERSION_TABLE = create_version_table();

} // namespace detail

/// Compile-time version string query
template<FixVersion Ver>
[[nodiscard]] consteval std::string_view version_string() noexcept {
    return detail::VersionInfo<Ver>::string;
}

/// Runtime version string query using O(1) lookup
[[nodiscard]] inline constexpr std::string_view version_string(FixVersion ver) noexcept {
    const auto idx = static_cast<uint8_t>(ver);
    if (idx < detail::VERSION_TABLE.size()) [[likely]] {
        return detail::VERSION_TABLE[idx].string;
    }
    return "Unknown";
}

/// Compile-time is_fixt query
template<FixVersion Ver>
[[nodiscard]] consteval bool is_fixt_version() noexcept {
    return detail::VersionInfo<Ver>::is_fixt;
}

/// Runtime is_fixt query using O(1) lookup
[[nodiscard]] inline constexpr bool is_fixt_version(FixVersion ver) noexcept {
    const auto idx = static_cast<uint8_t>(ver);
    if (idx < detail::VERSION_TABLE.size()) [[likely]] {
        return detail::VERSION_TABLE[idx].is_fixt;
    }
    return false;
}

/// Compile-time is_fix4 query
template<FixVersion Ver>
[[nodiscard]] consteval bool is_fix4_version() noexcept {
    return detail::VersionInfo<Ver>::is_fix4;
}

/// Runtime is_fix4 query using O(1) lookup
[[nodiscard]] inline constexpr bool is_fix4_version(FixVersion ver) noexcept {
    const auto idx = static_cast<uint8_t>(ver);
    if (idx < detail::VERSION_TABLE.size()) [[likely]] {
        return detail::VERSION_TABLE[idx].is_fix4;
    }
    return false;
}

/// Detect FIX version from BeginString
/// Note: This still uses if-chain as string comparison cannot use simple array lookup
[[nodiscard]] constexpr FixVersion detect_version(std::string_view begin_string) noexcept {
    // Optimize for common case: check length first
    if (begin_string.size() == 7) {
        // FIX.4.x versions (7 chars)
        if (begin_string[5] == '4') {
            const char minor = begin_string[6];
            if (minor >= '0' && minor <= '4') {
                return static_cast<FixVersion>(minor - '0' + 1);  // FIX_4_0=1, FIX_4_4=5
            }
        }
    } else if (begin_string.size() == 8) {
        // FIXT.1.1 (8 chars)
        if (begin_string == fix_version::FIXT_1_1) {
            return FixVersion::FIXT_1_1;
        }
    }
    return FixVersion::Unknown;
}

// Static assertions for FixVersion
static_assert(detail::VersionInfo<FixVersion::FIX_4_4>::string == "FIX.4.4");
static_assert(detail::VersionInfo<FixVersion::FIXT_1_1>::is_fixt == true);
static_assert(detail::VersionInfo<FixVersion::FIX_4_2>::is_fix4 == true);
static_assert(detail::VERSION_TABLE[5].string == "FIX.4.4");
static_assert(detail::VERSION_TABLE[6].is_fixt == true);

// ============================================================================
// FIX 5.0 Specific Tags
// ============================================================================

namespace tag {

// Application versioning tags (FIX 5.0+)
struct ApplVerID { static constexpr int value = 1128; };           // Application version per message
struct CstmApplVerID { static constexpr int value = 1129; };       // Custom application version
struct DefaultApplVerID { static constexpr int value = 1137; };    // Default app version in Logon
struct ApplExtID { static constexpr int value = 1156; };           // Application extension ID
struct DefaultApplExtID { static constexpr int value = 1407; };    // Default app extension in Logon
struct DefaultCstmApplVerID { static constexpr int value = 1408; };// Default custom app version

} // namespace tag

} // namespace nfx
