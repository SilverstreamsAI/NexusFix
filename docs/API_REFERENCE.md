# NexusFIX API Reference

High-performance FIX 4.4/5.0 protocol engine for quantitative trading.

---

## Quick Start

```cpp
#include <nexusfix/nexusfix.hpp>

using namespace nfx;
using namespace nfx::fix44;
```

---

## 1. Connecting to FIX Server

```cpp
// Create TCP transport
TcpTransport transport;
transport.connect("fix.broker.com", 9876);

// Create session
SessionConfig config{
    .sender_comp_id = "MY_CLIENT",
    .target_comp_id = "BROKER",
    .heartbeat_interval = 30
};
SessionManager session{transport, config};

// Logon
session.initiate_logon();

// Wait for session active
while (!session.is_active()) {
    session.poll();
}
```

---

## 2. Sending Orders

### NewOrderSingle (MsgType=D)

```cpp
MessageAssembler asm_;
NewOrderSingle::Builder order;

auto msg = order
    .sender_comp_id("MY_CLIENT")
    .target_comp_id("BROKER")
    .msg_seq_num(session.next_outbound_seq())
    .sending_time("20260122-10:00:00.000")
    .cl_ord_id("ORD001")              // Your order ID
    .symbol("AAPL")                   // Instrument
    .side(Side::Buy)                  // Buy or Sell
    .order_qty(Qty::from_int(100))    // Quantity
    .ord_type(OrdType::Limit)         // Market, Limit, Stop, etc.
    .price(FixedPrice::from_double(150.50))  // Price (for Limit orders)
    .time_in_force(TimeInForce::Day)  // Day, GTC, IOC, FOK
    .build(asm_);

transport.send(msg);
```

### OrderCancelRequest (MsgType=F)

```cpp
OrderCancelRequest::Builder cancel;

auto msg = cancel
    .sender_comp_id("MY_CLIENT")
    .target_comp_id("BROKER")
    .msg_seq_num(session.next_outbound_seq())
    .sending_time("20260122-10:00:01.000")
    .orig_cl_ord_id("ORD001")         // Original order to cancel
    .cl_ord_id("CXL001")              // Cancel request ID
    .symbol("AAPL")
    .side(Side::Buy)
    .transact_time("20260122-10:00:01.000")
    .build(asm_);

transport.send(msg);
```

---

## 3. Receiving Execution Reports

### ExecutionReport (MsgType=8)

```cpp
void on_message(std::span<const char> data) {
    auto result = ExecutionReport::from_buffer(data);
    if (!result) {
        // Parse error
        return;
    }

    auto& exec = *result;

    // Order identification
    exec.order_id;       // Exchange order ID
    exec.cl_ord_id;      // Your order ID
    exec.exec_id;        // Execution ID
    exec.symbol;         // Instrument

    // Status
    exec.exec_type;      // New, Fill, Canceled, Rejected, etc.
    exec.ord_status;     // Order current status

    // Quantities
    exec.order_qty;      // Original order quantity
    exec.cum_qty;        // Total filled quantity
    exec.leaves_qty;     // Remaining quantity

    // Fill details (when exec_type is Fill)
    exec.last_px;        // Fill price
    exec.last_qty;       // Fill quantity
    exec.avg_px;         // Average fill price

    // Convenience methods
    if (exec.is_fill()) {
        // Handle fill
        double fill_value = exec.last_px.to_double() * exec.last_qty.whole();
    }

    if (exec.is_rejected()) {
        // Handle rejection
        std::cout << "Rejected: " << exec.text << "\n";
    }

    if (exec.is_terminal()) {
        // Order is done (Filled, Canceled, Rejected, Expired)
    }
}
```

---

## 4. Subscribing to Market Data

### MarketDataRequest (MsgType=V)

```cpp
MarketDataRequest::Builder req;

auto msg = req
    .sender_comp_id("MY_CLIENT")
    .target_comp_id("BROKER")
    .msg_seq_num(session.next_outbound_seq())
    .sending_time("20260122-10:00:00.000")
    .md_req_id("MD001")               // Your request ID
    .subscription_type(SubscriptionRequestType::SnapshotPlusUpdates)  // Subscribe
    .market_depth(5)                  // Top 5 levels (0 = full book)
    .md_update_type(MDUpdateType::IncrementalRefresh)
    // What data to receive
    .add_entry_type(MDEntryType::Bid)
    .add_entry_type(MDEntryType::Offer)
    .add_entry_type(MDEntryType::Trade)
    // Symbols to subscribe
    .add_symbol("AAPL")
    .add_symbol("GOOGL")
    .add_symbol("MSFT")
    .build(asm_);

transport.send(msg);
```

### Unsubscribe

```cpp
auto msg = req
    .md_req_id("MD001")
    .subscription_type(SubscriptionRequestType::DisablePreviousSnapshot)
    .add_symbol("AAPL")
    .build(asm_);
```

---

## 5. Receiving Market Data

### MarketDataSnapshotFullRefresh (MsgType=W)

```cpp
void on_snapshot(std::span<const char> data) {
    auto result = MarketDataSnapshotFullRefresh::from_buffer(data);
    if (!result) return;

    auto& snapshot = *result;

    std::cout << "Symbol: " << snapshot.symbol << "\n";
    std::cout << "Entries: " << snapshot.entry_count() << "\n";

    // Iterate market data entries
    for (auto iter = snapshot.entries(); iter.has_next(); ) {
        MDEntry entry = iter.next();

        if (entry.is_bid()) {
            double price = FixedPrice{entry.price_raw}.to_double();
            int64_t size = Qty{entry.size_raw}.whole();
            std::cout << "  Bid: " << price << " x " << size << "\n";
        }
        else if (entry.is_offer()) {
            double price = FixedPrice{entry.price_raw}.to_double();
            int64_t size = Qty{entry.size_raw}.whole();
            std::cout << "  Ask: " << price << " x " << size << "\n";
        }
        else if (entry.is_trade()) {
            std::cout << "  Trade: " << FixedPrice{entry.price_raw}.to_double() << "\n";
        }
    }
}
```

### MarketDataIncrementalRefresh (MsgType=X)

```cpp
void on_incremental(std::span<const char> data) {
    auto result = MarketDataIncrementalRefresh::from_buffer(data);
    if (!result) return;

    auto& update = *result;

    for (auto iter = update.entries(); iter.has_next(); ) {
        MDEntry entry = iter.next();

        switch (entry.update_action) {
            case MDUpdateAction::New:
                // New price level
                add_level(entry.symbol, entry.entry_type,
                         entry.price_raw, entry.size_raw, entry.position_no);
                break;

            case MDUpdateAction::Change:
                // Price/size changed
                update_level(entry.symbol, entry.entry_type,
                            entry.price_raw, entry.size_raw, entry.position_no);
                break;

            case MDUpdateAction::Delete:
                // Level removed
                delete_level(entry.symbol, entry.entry_type, entry.position_no);
                break;
        }
    }
}
```

### MarketDataRequestReject (MsgType=Y)

```cpp
void on_md_reject(std::span<const char> data) {
    auto result = MarketDataRequestReject::from_buffer(data);
    if (!result) return;

    auto& reject = *result;

    std::cout << "Request " << reject.md_req_id << " rejected\n";
    std::cout << "Reason: " << reject.rejection_reason_name() << "\n";
    std::cout << "Text: " << reject.text << "\n";
}
```

---

## 6. Message Routing

```cpp
void on_message(std::span<const char> data) {
    // Fast message type extraction
    auto parser = IndexedParser::parse(data);
    if (!parser) return;

    char msg_type = parser->msg_type();

    switch (msg_type) {
        // Session messages
        case '0': on_heartbeat(data); break;
        case '1': on_test_request(data); break;
        case 'A': on_logon(data); break;
        case '5': on_logout(data); break;

        // Order messages
        case '8': on_execution_report(data); break;
        case '9': on_order_cancel_reject(data); break;

        // Market data messages
        case 'W': on_snapshot(data); break;
        case 'X': on_incremental(data); break;
        case 'Y': on_md_reject(data); break;
    }
}
```

---

## 7. Types Reference

### Price & Quantity

```cpp
// Fixed-point price (8 decimal places)
FixedPrice price = FixedPrice::from_double(150.50);
FixedPrice price = FixedPrice::from_string("150.50");
double d = price.to_double();

// Quantity (4 decimal places for fractional shares)
Qty qty = Qty::from_int(100);
Qty qty = Qty::from_double(100.5);
int64_t whole = qty.whole();

// User-defined literals
using namespace nfx::literals;
auto price = 150.50_price;
auto qty = 100_qty;
```

### Order Enums

```cpp
// Side (Tag 54)
Side::Buy         // '1'
Side::Sell        // '2'
Side::SellShort   // '5'

// Order Type (Tag 40)
OrdType::Market       // '1'
OrdType::Limit        // '2'
OrdType::Stop         // '3'
OrdType::StopLimit    // '4'

// Time In Force (Tag 59)
TimeInForce::Day              // '0'
TimeInForce::GoodTillCancel   // '1'
TimeInForce::ImmediateOrCancel // '3'
TimeInForce::FillOrKill       // '4'

// Order Status (Tag 39)
OrdStatus::New              // '0'
OrdStatus::PartiallyFilled  // '1'
OrdStatus::Filled           // '2'
OrdStatus::Canceled         // '4'
OrdStatus::Rejected         // '8'

// Execution Type (Tag 150)
ExecType::New         // '0'
ExecType::Fill        // '2'
ExecType::Canceled    // '4'
ExecType::Rejected    // '8'
ExecType::Trade       // 'F'
```

### Market Data Enums

```cpp
// MD Entry Type (Tag 269)
MDEntryType::Bid              // '0'
MDEntryType::Offer            // '1'
MDEntryType::Trade            // '2'
MDEntryType::OpeningPrice     // '4'
MDEntryType::ClosingPrice     // '5'
MDEntryType::SettlementPrice  // '6'

// MD Update Action (Tag 279)
MDUpdateAction::New     // '0'
MDUpdateAction::Change  // '1'
MDUpdateAction::Delete  // '2'

// Subscription Type (Tag 263)
SubscriptionRequestType::Snapshot              // '0'
SubscriptionRequestType::SnapshotPlusUpdates   // '1' (Subscribe)
SubscriptionRequestType::DisablePreviousSnapshot // '2' (Unsubscribe)
```

---

## 8. Error Handling

```cpp
// All parse functions return std::expected
auto result = ExecutionReport::from_buffer(data);

if (result.has_value()) {
    // Success
    auto& msg = *result;
} else {
    // Error
    ParseError err = result.error();
    std::cout << "Error: " << err.message() << "\n";
    std::cout << "Tag: " << err.tag << "\n";
}
```

---

## 9. Session Management

```cpp
SessionManager session{transport, config};

// Lifecycle
session.initiate_logon();
session.initiate_logout("Normal termination");

// State
session.is_active();      // Session ready for trading
session.is_logged_in();   // Logon completed
session.current_state();  // SessionState enum

// Sequence numbers
session.next_outbound_seq();
session.expected_inbound_seq();

// Heartbeat (call periodically)
session.on_timer_tick();

// Process incoming data
session.on_data_received(data);
```

---

## 10. Complete Example

```cpp
#include <nexusfix/nexusfix.hpp>
#include <iostream>

using namespace nfx;
using namespace nfx::fix44;

int main() {
    // Connect
    TcpTransport transport;
    if (!transport.connect("fix.broker.com", 9876)) {
        std::cerr << "Connection failed\n";
        return 1;
    }

    // Session setup
    SessionConfig config{
        .sender_comp_id = "MY_CLIENT",
        .target_comp_id = "BROKER",
        .heartbeat_interval = 30
    };
    SessionManager session{transport, config};
    session.initiate_logon();

    // Wait for active
    while (!session.is_active()) {
        session.poll();
    }
    std::cout << "Session active!\n";

    // Subscribe to market data
    MessageAssembler asm_;
    MarketDataRequest::Builder md_req;
    auto md_msg = md_req
        .md_req_id("MD001")
        .subscription_type(SubscriptionRequestType::SnapshotPlusUpdates)
        .market_depth(5)
        .add_entry_type(MDEntryType::Bid)
        .add_entry_type(MDEntryType::Offer)
        .add_symbol("AAPL")
        .build(asm_);
    transport.send(md_msg);

    // Send order
    NewOrderSingle::Builder order;
    auto order_msg = order
        .cl_ord_id("ORD001")
        .symbol("AAPL")
        .side(Side::Buy)
        .order_qty(Qty::from_int(100))
        .ord_type(OrdType::Limit)
        .price(FixedPrice::from_double(150.00))
        .build(asm_);
    transport.send(order_msg);

    // Main loop
    while (session.is_active()) {
        auto data = transport.receive();
        if (!data.empty()) {
            // Route message
            auto parser = IndexedParser::parse(data);
            if (parser) {
                switch (parser->msg_type()) {
                    case '8': {
                        auto exec = ExecutionReport::from_buffer(data);
                        if (exec && exec->is_fill()) {
                            std::cout << "Fill: " << exec->last_qty.whole()
                                      << " @ " << exec->last_px.to_double() << "\n";
                        }
                        break;
                    }
                    case 'W': {
                        auto snap = MarketDataSnapshotFullRefresh::from_buffer(data);
                        if (snap) {
                            std::cout << snap->symbol << " snapshot received\n";
                        }
                        break;
                    }
                }
            }
        }
        session.on_timer_tick();
    }

    return 0;
}
```

---

## Performance

| Operation | Latency |
|-----------|---------|
| ExecutionReport parse | 246 ns |
| NewOrderSingle parse | 229 ns |
| Field access (4 fields) | 11 ns |
| Throughput | 4.17M msg/sec |

**3x faster than QuickFIX**

---

## Message Types Summary

| MsgType | Name | Direction | Purpose |
|---------|------|-----------|---------|
| A | Logon | Both | Session authentication |
| 5 | Logout | Both | Session termination |
| 0 | Heartbeat | Both | Keepalive |
| D | NewOrderSingle | Send | Submit order |
| F | OrderCancelRequest | Send | Cancel order |
| 8 | ExecutionReport | Receive | Order status/fills |
| 9 | OrderCancelReject | Receive | Cancel rejected |
| V | MarketDataRequest | Send | Subscribe to data |
| W | MarketDataSnapshotFullRefresh | Receive | Full snapshot |
| X | MarketDataIncrementalRefresh | Receive | Incremental update |
| Y | MarketDataRequestReject | Receive | Subscription rejected |
