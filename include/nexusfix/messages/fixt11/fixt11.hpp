#pragma once

// FIXT 1.1 Transport Layer Messages
// Used with FIX 5.0+ application messages

#include "nexusfix/messages/fixt11/logon.hpp"
#include "nexusfix/messages/fixt11/session.hpp"

namespace nfx::fixt11 {

// All FIXT 1.1 session message types:
// - Logon (A) - logon.hpp
// - Logout (5) - logon.hpp
// - Heartbeat (0) - session.hpp
// - TestRequest (1) - session.hpp
// - ResendRequest (2) - session.hpp
// - SequenceReset (4) - session.hpp
// - Reject (3) - session.hpp

} // namespace nfx::fixt11
