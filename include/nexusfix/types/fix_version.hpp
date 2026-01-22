#pragma once

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

    /// Convert ApplVerID to string
    [[nodiscard]] constexpr std::string_view to_string(char ver) noexcept {
        switch (ver) {
            case FIX_2_7:     return "FIX.2.7";
            case FIX_3_0:     return "FIX.3.0";
            case FIX_4_0:     return fix_version::FIX_4_0;
            case FIX_4_1:     return fix_version::FIX_4_1;
            case FIX_4_2:     return fix_version::FIX_4_2;
            case FIX_4_3:     return fix_version::FIX_4_3;
            case FIX_4_4:     return fix_version::FIX_4_4;
            case FIX_5_0:     return fix_version::FIX_5_0;
            case FIX_5_0_SP1: return fix_version::FIX_5_0_SP1;
            case FIX_5_0_SP2: return fix_version::FIX_5_0_SP2;
            default:          return "Unknown";
        }
    }
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

/// Detect FIX version from BeginString
[[nodiscard]] constexpr FixVersion detect_version(std::string_view begin_string) noexcept {
    if (begin_string == fix_version::FIX_4_0) return FixVersion::FIX_4_0;
    if (begin_string == fix_version::FIX_4_1) return FixVersion::FIX_4_1;
    if (begin_string == fix_version::FIX_4_2) return FixVersion::FIX_4_2;
    if (begin_string == fix_version::FIX_4_3) return FixVersion::FIX_4_3;
    if (begin_string == fix_version::FIX_4_4) return FixVersion::FIX_4_4;
    if (begin_string == fix_version::FIXT_1_1) return FixVersion::FIXT_1_1;
    return FixVersion::Unknown;
}

/// Check if version uses FIXT transport (FIX 5.0+)
[[nodiscard]] constexpr bool is_fixt_version(FixVersion ver) noexcept {
    return ver == FixVersion::FIXT_1_1 ||
           ver == FixVersion::FIX_5_0 ||
           ver == FixVersion::FIX_5_0_SP1 ||
           ver == FixVersion::FIX_5_0_SP2;
}

/// Check if version is FIX 4.x
[[nodiscard]] constexpr bool is_fix4_version(FixVersion ver) noexcept {
    return ver >= FixVersion::FIX_4_0 && ver <= FixVersion::FIX_4_4;
}

/// Get version string
[[nodiscard]] constexpr std::string_view version_string(FixVersion ver) noexcept {
    switch (ver) {
        case FixVersion::FIX_4_0:     return fix_version::FIX_4_0;
        case FixVersion::FIX_4_1:     return fix_version::FIX_4_1;
        case FixVersion::FIX_4_2:     return fix_version::FIX_4_2;
        case FixVersion::FIX_4_3:     return fix_version::FIX_4_3;
        case FixVersion::FIX_4_4:     return fix_version::FIX_4_4;
        case FixVersion::FIXT_1_1:    return fix_version::FIXT_1_1;
        case FixVersion::FIX_5_0:     return fix_version::FIX_5_0;
        case FixVersion::FIX_5_0_SP1: return fix_version::FIX_5_0_SP1;
        case FixVersion::FIX_5_0_SP2: return fix_version::FIX_5_0_SP2;
        default:                       return "Unknown";
    }
}

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
