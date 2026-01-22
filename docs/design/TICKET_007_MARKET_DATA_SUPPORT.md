# TICKET_007: FIX Market Data Message Support

## Overview

Implement FIX 4.4 Market Data message types to enable real-time market data subscription and reception from exchanges, brokers, and liquidity providers.

**Priority:** High
**Estimated Effort:** 2-3 days
**Dependencies:** TICKET_005 (Core implementation complete)

---

## Objective

Enable NexusFIX to:
1. Subscribe to market data (quotes, trades, order book)
2. Receive real-time market data snapshots and incremental updates
3. Handle subscription rejections gracefully
4. Support Repeating Groups for multi-symbol subscriptions

---

## FIX 4.4 Market Data Messages

### Messages to Implement

| MsgType | Message Name | Direction | Description |
|---------|--------------|-----------|-------------|
| V | MarketDataRequest | Client -> Server | Subscribe/unsubscribe to market data |
| W | MarketDataSnapshotFullRefresh | Server -> Client | Full market data snapshot |
| X | MarketDataIncrementalRefresh | Server -> Client | Incremental market data update |
| Y | MarketDataRequestReject | Server -> Client | Subscription rejection |

---

## Phase 1: MarketDataRequest (MsgType=V)

### Required Fields

| Tag | Field Name | Required | Description |
|-----|------------|----------|-------------|
| 262 | MDReqID | Y | Unique request identifier |
| 263 | SubscriptionRequestType | Y | 0=Snapshot, 1=Subscribe, 2=Unsubscribe |
| 264 | MarketDepth | Y | 0=Full book, 1=Top of book, N=N levels |
| 265 | MDUpdateType | C | 0=Full refresh, 1=Incremental (required if 263=1) |
| 266 | AggregatedBook | N | Y=Aggregated, N=Non-aggregated |
| 267 | NoMDEntryTypes | Y | Number of MDEntryType entries (Repeating Group) |
| 269 | MDEntryType | Y | 0=Bid, 1=Offer, 2=Trade, etc. |
| 146 | NoRelatedSym | Y | Number of symbols (Repeating Group) |
| 55 | Symbol | Y | Instrument symbol |

### Repeating Groups Required

```cpp
// MDEntryTypes group (tag 267)
struct MDEntryTypeGroup {
    char entry_type;  // Tag 269: '0'=Bid, '1'=Offer, '2'=Trade
};

// RelatedSym group (tag 146)
struct RelatedSymGroup {
    std::string_view symbol;           // Tag 55
    std::optional<std::string_view> security_id;  // Tag 48
    std::optional<std::string_view> security_exchange;  // Tag 207
};
```

### Message Builder API

```cpp
namespace nexusfix::fix44 {

class MarketDataRequest {
public:
    // Builder pattern
    MarketDataRequest& md_req_id(std::string_view id);
    MarketDataRequest& subscription_type(SubscriptionRequestType type);
    MarketDataRequest& market_depth(int depth);
    MarketDataRequest& update_type(MDUpdateType type);

    // Repeating groups
    MarketDataRequest& add_entry_type(MDEntryType type);
    MarketDataRequest& add_symbol(std::string_view symbol);

    // Build message
    std::span<const char> build(std::span<char> buffer) const;

    // Subscription types
    enum class SubscriptionRequestType : char {
        Snapshot = '0',
        Subscribe = '1',
        Unsubscribe = '2'
    };

    // Entry types
    enum class MDEntryType : char {
        Bid = '0',
        Offer = '1',
        Trade = '2',
        IndexValue = '3',
        OpeningPrice = '4',
        ClosingPrice = '5',
        SettlementPrice = '6',
        TradingSessionHighPrice = '7',
        TradingSessionLowPrice = '8',
        TradingSessionVWAPPrice = '9'
    };
};

} // namespace nexusfix::fix44
```

---

## Phase 2: MarketDataSnapshotFullRefresh (MsgType=W)

### Required Fields

| Tag | Field Name | Required | Description |
|-----|------------|----------|-------------|
| 262 | MDReqID | C | Request ID (if in response to request) |
| 55 | Symbol | Y | Instrument symbol |
| 268 | NoMDEntries | Y | Number of market data entries |
| 269 | MDEntryType | Y | Entry type (Bid/Offer/Trade) |
| 270 | MDEntryPx | C | Price |
| 271 | MDEntrySize | C | Quantity |
| 272 | MDEntryDate | N | Date of entry |
| 273 | MDEntryTime | N | Time of entry |

### Repeating Group: MDEntries

```cpp
struct MDEntry {
    MDEntryType entry_type;        // Tag 269
    FixedPrice price;              // Tag 270
    Qty size;                      // Tag 271
    std::optional<std::string_view> entry_id;  // Tag 278
    std::optional<int> position;   // Tag 290 (price level)
};
```

### Parser API

```cpp
namespace nexusfix::fix44 {

class MarketDataSnapshotFullRefresh {
public:
    // Parse from raw FIX message
    static ParseResult<MarketDataSnapshotFullRefresh>
    parse(std::span<const char> data);

    // Accessors
    [[nodiscard]] std::string_view md_req_id() const;
    [[nodiscard]] std::string_view symbol() const;
    [[nodiscard]] size_t entry_count() const;

    // Iterate entries (zero-copy)
    [[nodiscard]] MDEntryIterator begin() const;
    [[nodiscard]] MDEntryIterator end() const;

    // Direct access
    [[nodiscard]] std::optional<MDEntry> get_best_bid() const;
    [[nodiscard]] std::optional<MDEntry> get_best_offer() const;
    [[nodiscard]] std::span<const MDEntry> get_trades() const;
};

} // namespace nexusfix::fix44
```

---

## Phase 3: MarketDataIncrementalRefresh (MsgType=X)

### Required Fields

| Tag | Field Name | Required | Description |
|-----|------------|----------|-------------|
| 262 | MDReqID | N | Request ID |
| 268 | NoMDEntries | Y | Number of entries |
| 279 | MDUpdateAction | Y | 0=New, 1=Change, 2=Delete |
| 269 | MDEntryType | Y | Entry type |
| 55 | Symbol | C | Symbol (may be in group) |
| 270 | MDEntryPx | C | Price |
| 271 | MDEntrySize | C | Size |

### Update Actions

```cpp
enum class MDUpdateAction : char {
    New = '0',
    Change = '1',
    Delete = '2',
    DeleteThru = '3',
    DeleteFrom = '4'
};
```

### Parser API

```cpp
namespace nexusfix::fix44 {

class MarketDataIncrementalRefresh {
public:
    static ParseResult<MarketDataIncrementalRefresh>
    parse(std::span<const char> data);

    // Iterate updates
    class UpdateIterator {
    public:
        struct Update {
            MDUpdateAction action;
            MDEntryType entry_type;
            std::string_view symbol;
            std::optional<FixedPrice> price;
            std::optional<Qty> size;
            std::optional<int> position;
        };

        Update operator*() const;
        UpdateIterator& operator++();
        bool operator!=(const UpdateIterator&) const;
    };

    [[nodiscard]] UpdateIterator begin() const;
    [[nodiscard]] UpdateIterator end() const;
};

} // namespace nexusfix::fix44
```

---

## Phase 4: MarketDataRequestReject (MsgType=Y)

### Required Fields

| Tag | Field Name | Required | Description |
|-----|------------|----------|-------------|
| 262 | MDReqID | Y | Original request ID |
| 281 | MDReqRejReason | N | Rejection reason code |
| 58 | Text | N | Free-form rejection text |

### Rejection Reasons

```cpp
enum class MDReqRejReason : char {
    UnknownSymbol = '0',
    DuplicateMDReqID = '1',
    InsufficientPermissions = '2',
    UnsupportedSubscriptionType = '3',
    UnsupportedMarketDepth = '4',
    UnsupportedMDUpdateType = '5',
    UnsupportedAggregatedBook = '6',
    UnsupportedMDEntryType = '7',
    UnsupportedTradingSessionID = '8',
    UnsupportedScope = '9',
    UnsupportedOpenCloseSettleFlag = 'A',
    UnsupportedMDImplicitDelete = 'B',
    InsufficientCredit = 'C',
    Other = 'D'
};
```

---

## Phase 5: Repeating Groups Infrastructure

### Generic Repeating Group Parser

```cpp
namespace nexusfix::parser {

template <typename EntryT>
class RepeatingGroupIterator {
public:
    using value_type = EntryT;

    RepeatingGroupIterator(std::span<const char> data,
                          int count_tag,
                          std::span<const int> entry_tags);

    [[nodiscard]] EntryT operator*() const;
    RepeatingGroupIterator& operator++();
    [[nodiscard]] bool operator!=(const RepeatingGroupIterator&) const;

private:
    std::span<const char> data_;
    size_t current_pos_;
    size_t current_entry_;
    size_t total_entries_;
};

// Helper to define repeating group schema
template <int CountTag, int... EntryTags>
struct RepeatingGroupSchema {
    static constexpr int count_tag = CountTag;
    static constexpr std::array<int, sizeof...(EntryTags)> entry_tags = {EntryTags...};
};

// MDEntries schema: 268=NoMDEntries, followed by 269, 270, 271, etc.
using MDEntriesSchema = RepeatingGroupSchema<268, 269, 270, 271, 272, 273, 278, 290>;

} // namespace nexusfix::parser
```

---

## Implementation Plan

### Week 1: Core Infrastructure

| Task | Files | Effort |
|------|-------|--------|
| Repeating Group parser | `parser/repeating_group.hpp` | 4h |
| MDEntryType enum | `types/market_data_types.hpp` | 1h |
| Unit tests for groups | `tests/test_repeating_group.cpp` | 2h |

### Week 2: Message Types

| Task | Files | Effort |
|------|-------|--------|
| MarketDataRequest builder | `messages/fix44/market_data_request.hpp` | 3h |
| MarketDataSnapshotFullRefresh parser | `messages/fix44/market_data_snapshot.hpp` | 3h |
| MarketDataIncrementalRefresh parser | `messages/fix44/market_data_incremental.hpp` | 3h |
| MarketDataRequestReject parser | `messages/fix44/market_data_reject.hpp` | 1h |

### Week 3: Integration & Testing

| Task | Files | Effort |
|------|-------|--------|
| Session manager integration | `session/session_manager.hpp` | 2h |
| Market data example client | `examples/market_data_client.cpp` | 3h |
| Benchmark vs QuickFIX | `benchmarks/market_data_bench.cpp` | 2h |
| Documentation | `docs/MARKET_DATA_USAGE.md` | 1h |

---

## File Structure

```
include/nexusfix/
├── types/
│   └── market_data_types.hpp      # MDEntryType, MDUpdateAction, etc.
├── parser/
│   └── repeating_group.hpp        # Generic repeating group parser
├── messages/
│   └── fix44/
│       ├── market_data_request.hpp
│       ├── market_data_snapshot.hpp
│       ├── market_data_incremental.hpp
│       └── market_data_reject.hpp
examples/
└── market_data_client.cpp          # Example market data subscriber
tests/
├── test_repeating_group.cpp
└── test_market_data_messages.cpp
```

---

## Performance Targets

| Metric | Target | Notes |
|--------|--------|-------|
| MarketDataSnapshot parse | < 300 ns | With 10 MDEntries |
| MarketDataIncremental parse | < 200 ns | With 5 updates |
| Repeating group iteration | < 20 ns/entry | Zero-copy |
| Memory allocation | 0 | Hot path |

---

## Success Criteria

1. **Functional**
   - [ ] Can subscribe to market data from FIX server
   - [ ] Can receive and parse full snapshots
   - [ ] Can receive and parse incremental updates
   - [ ] Can handle subscription rejections
   - [ ] Repeating groups work correctly

2. **Performance**
   - [ ] Parse latency within targets
   - [ ] Zero allocation on hot path
   - [ ] Faster than QuickFIX

3. **Quality**
   - [ ] Unit tests for all message types
   - [ ] Integration test with mock FIX server
   - [ ] Example client working

---

## Example Usage

```cpp
#include <nexusfix/messages/fix44/market_data_request.hpp>
#include <nexusfix/messages/fix44/market_data_snapshot.hpp>

using namespace nexusfix::fix44;

// Subscribe to market data
MarketDataRequest req;
req.md_req_id("MD001")
   .subscription_type(SubscriptionRequestType::Subscribe)
   .market_depth(5)  // Top 5 levels
   .update_type(MDUpdateType::Incremental)
   .add_entry_type(MDEntryType::Bid)
   .add_entry_type(MDEntryType::Offer)
   .add_entry_type(MDEntryType::Trade)
   .add_symbol("AAPL")
   .add_symbol("GOOGL")
   .add_symbol("MSFT");

auto msg = req.build(buffer);
session.send(msg);

// Handle incoming market data
void on_message(std::span<const char> data) {
    auto msg_type = get_msg_type(data);

    if (msg_type == "W") {
        auto snapshot = MarketDataSnapshotFullRefresh::parse(data);
        if (snapshot) {
            std::cout << "Symbol: " << snapshot->symbol() << "\n";
            for (const auto& entry : *snapshot) {
                if (entry.entry_type == MDEntryType::Bid) {
                    std::cout << "  Bid: " << entry.price << " x " << entry.size << "\n";
                }
            }
        }
    }
    else if (msg_type == "X") {
        auto incremental = MarketDataIncrementalRefresh::parse(data);
        if (incremental) {
            for (const auto& update : *incremental) {
                apply_update(update);
            }
        }
    }
}
```

---

## References

- FIX 4.4 Specification: Market Data Messages
- TICKET_005: NexusFIX Implementation Summary
- QuickFIX Market Data implementation (for comparison)
