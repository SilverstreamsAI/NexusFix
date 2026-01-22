#pragma once

// FIX 5.0 Application Layer Messages
// Used with FIXT 1.1 transport layer (BeginString = "FIXT.1.1")

#include "nexusfix/messages/fix50/new_order_single.hpp"
#include "nexusfix/messages/fix50/execution_report.hpp"

namespace nfx::fix50 {

// All FIX 5.0 application message types:
// - NewOrderSingle (D) - new_order_single.hpp
// - OrderCancelRequest (F) - new_order_single.hpp
// - ExecutionReport (8) - execution_report.hpp
// - OrderCancelReject (9) - execution_report.hpp

// Note: FIX 5.0 application messages use FIXT.1.1 as BeginString
// and can optionally include ApplVerID (tag 1128) for per-message
// application version specification.
//
// Session messages (Logon, Logout, Heartbeat, etc.) are in the
// fixt11 namespace and always use FIXT.1.1.

} // namespace nfx::fix50
