# NexusFIX Quick Start

High-performance FIX protocol engine, 3x faster than QuickFIX.

---

## Headers

```cpp
#include <nexusfix/nexusfix.hpp>

using namespace nfx;
using namespace nfx::fix44;
```

---

## 1. Connecting to Server

```cpp
TcpTransport transport;
transport.connect("fix.broker.com", 9876);

SessionConfig config{
    .sender_comp_id = "MY_CLIENT",
    .target_comp_id = "BROKER",
    .heartbeat_interval = 30
};

SessionManager session{transport, config};
session.initiate_logon();

while (!session.is_active()) {
    session.poll();
}
// Connected!
```

---

## 2. Sending an Order

```cpp
MessageAssembler asm_;
NewOrderSingle::Builder order;

auto msg = order
    .cl_ord_id("ORD001")           // Order ID
    .symbol("AAPL")                // Symbol
    .side(Side::Buy)               // Side
    .order_qty(Qty::from_int(100)) // Quantity
    .ord_type(OrdType::Limit)      // Order Type
    .price(FixedPrice::from_double(150.50))  // Price
    .build(asm_);

transport.send(msg);
```

---

## 3. Canceling an Order

```cpp
OrderCancelRequest::Builder cancel;

auto msg = cancel
    .orig_cl_ord_id("ORD001")      // Original Order ID to cancel
    .cl_ord_id("CXL001")           // Cancel Request ID
    .symbol("AAPL")
    .side(Side::Buy)
    .build(asm_);

transport.send(msg);
```

---

## 4. Receiving Execution Reports

```cpp
void on_execution(std::span<const char> data) {
    auto result = ExecutionReport::from_buffer(data);
    if (!result) return;

    auto& exec = *result;

    // Check status
    if (exec.is_fill()) {
        // Filled!
        std::cout << "Fill: " << exec.last_qty.whole()
                  << " shares @ " << exec.last_px.to_double() << "\n";
    }
    else if (exec.is_rejected()) {
        // Rejected
        std::cout << "Rejected: " << exec.text << "\n";
    }

    // Order status
    std::cout << "CumQty: " << exec.cum_qty.whole() << "\n";
    std::cout << "LeavesQty: " << exec.leaves_qty.whole() << "\n";
}
```

---

## 5. Subscribing to Market Data

```cpp
MarketDataRequest::Builder req;

auto msg = req
    .md_req_id("MD001")
    .subscription_type(SubscriptionRequestType::SnapshotPlusUpdates)
    .market_depth(5)               // Top 5 levels
    .add_entry_type(MDEntryType::Bid)    // Bid
    .add_entry_type(MDEntryType::Offer)  // Offer
    .add_entry_type(MDEntryType::Trade)  // Trade
    .add_symbol("AAPL")
    .add_symbol("GOOGL")
    .build(asm_);

transport.send(msg);
```

---

## 6. Receiving Market Data Snapshots

```cpp
void on_snapshot(std::span<const char> data) {
    auto result = MarketDataSnapshotFullRefresh::from_buffer(data);
    if (!result) return;

    auto& snap = *result;
    std::cout << "Symbol: " << snap.symbol << "\n";

    for (auto iter = snap.entries(); iter.has_next(); ) {
        MDEntry e = iter.next();
        double price = FixedPrice{e.price_raw}.to_double();
        int64_t size = Qty{e.size_raw}.whole();

        if (e.is_bid()) {
            std::cout << "  Bid: " << price << " x " << size << "\n";
        } else if (e.is_offer()) {
            std::cout << "  Ask: " << price << " x " << size << "\n";
        }
    }
}
```

---

## 7. Receiving Market Data Updates

```cpp
void on_update(std::span<const char> data) {
    auto result = MarketDataIncrementalRefresh::from_buffer(data);
    if (!result) return;

    for (auto iter = result->entries(); iter.has_next(); ) {
        MDEntry e = iter.next();

        switch (e.update_action) {
            case MDUpdateAction::New:    // New
            case MDUpdateAction::Change: // Change
            case MDUpdateAction::Delete: // Delete
                update_orderbook(e);
                break;
        }
    }
}
```

---

## 8. Message Routing

```cpp
void on_message(std::span<const char> data) {
    auto parser = IndexedParser::parse(data);
    if (!parser) return;

    switch (parser->msg_type()) {
        case '8': on_execution(data);  break;  // Execution Report
        case '9': on_cancel_reject(data); break;  // Cancel Reject
        case 'W': on_snapshot(data);   break;  // Market Data Snapshot
        case 'X': on_update(data);     break;  // Market Data Update
        case 'Y': on_md_reject(data);  break;  // Market Data Reject
    }
}
```

---

## Common Enums

### Side

| Value | Meaning |
|-------|---------|
| `Side::Buy` | Buy |
| `Side::Sell` | Sell |
| `Side::SellShort` | Sell Short |

### OrdType

| Value | Meaning |
|-------|---------|
| `OrdType::Market` | Market |
| `OrdType::Limit` | Limit |
| `OrdType::Stop` | Stop |
| `OrdType::StopLimit` | Stop Limit |

### OrdStatus

| Value | Meaning |
|-------|---------|
| `OrdStatus::New` | New |
| `OrdStatus::PartiallyFilled` | Partially Filled |
| `OrdStatus::Filled` | Filled |
| `OrdStatus::Canceled` | Canceled |
| `OrdStatus::Rejected` | Rejected |

### MDEntryType

| Value | Meaning |
|-------|---------|
| `MDEntryType::Bid` | Bid |
| `MDEntryType::Offer` | Offer |
| `MDEntryType::Trade` | Trade |
| `MDEntryType::OpeningPrice` | Opening Price |
| `MDEntryType::ClosingPrice` | Closing Price |

---

## Performance Metrics

| Operation | Latency |
|-----------|---------|
| Parse ExecutionReport | 246 ns |
| Parse NewOrderSingle | 229 ns |
| Field Access | 11 ns |
| Throughput | 4.17M msg/sec |

**3x faster than QuickFIX**

---

## Message Type Quick Reference

| Type | Name | Direction | Purpose |
|------|------|-----------|---------|
| D | NewOrderSingle | Send | Order Placement |
| F | OrderCancelRequest | Send | Order Cancellation |
| 8 | ExecutionReport | Receive | Execution Status |
| V | MarketDataRequest | Send | MD Subscription |
| W | MarketDataSnapshotFullRefresh | Receive | Full Snapshot |
| X | MarketDataIncrementalRefresh | Receive | Incremental Update |

```