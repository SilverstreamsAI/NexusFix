# NexusFIX 快速入门

高性能 FIX 协议引擎，比 QuickFIX 快 3 倍。

---

## 头文件

```cpp
#include <nexusfix/nexusfix.hpp>

using namespace nfx;
using namespace nfx::fix44;
```

---

## 1. 连接服务器

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
// 连接成功!
```

---

## 2. 发送订单

```cpp
MessageAssembler asm_;
NewOrderSingle::Builder order;

auto msg = order
    .cl_ord_id("ORD001")           // 订单ID
    .symbol("AAPL")                // 股票代码
    .side(Side::Buy)               // 买入
    .order_qty(Qty::from_int(100)) // 数量
    .ord_type(OrdType::Limit)      // 限价单
    .price(FixedPrice::from_double(150.50))  // 价格
    .build(asm_);

transport.send(msg);
```

---

## 3. 撤销订单

```cpp
OrderCancelRequest::Builder cancel;

auto msg = cancel
    .orig_cl_ord_id("ORD001")      // 要撤销的订单
    .cl_ord_id("CXL001")           // 撤单请求ID
    .symbol("AAPL")
    .side(Side::Buy)
    .build(asm_);

transport.send(msg);
```

---

## 4. 接收成交回报

```cpp
void on_execution(std::span<const char> data) {
    auto result = ExecutionReport::from_buffer(data);
    if (!result) return;

    auto& exec = *result;

    // 检查状态
    if (exec.is_fill()) {
        // 成交了!
        std::cout << "成交: " << exec.last_qty.whole()
                  << " 股 @ " << exec.last_px.to_double() << "\n";
    }
    else if (exec.is_rejected()) {
        // 被拒绝
        std::cout << "拒绝: " << exec.text << "\n";
    }

    // 订单状态
    std::cout << "累计成交: " << exec.cum_qty.whole() << "\n";
    std::cout << "剩余数量: " << exec.leaves_qty.whole() << "\n";
}
```

---

## 5. 订阅行情

```cpp
MarketDataRequest::Builder req;

auto msg = req
    .md_req_id("MD001")
    .subscription_type(SubscriptionRequestType::SnapshotPlusUpdates)
    .market_depth(5)               // Top 5 档
    .add_entry_type(MDEntryType::Bid)    // 买盘
    .add_entry_type(MDEntryType::Offer)  // 卖盘
    .add_entry_type(MDEntryType::Trade)  // 成交
    .add_symbol("AAPL")
    .add_symbol("GOOGL")
    .build(asm_);

transport.send(msg);
```

---

## 6. 接收行情快照

```cpp
void on_snapshot(std::span<const char> data) {
    auto result = MarketDataSnapshotFullRefresh::from_buffer(data);
    if (!result) return;

    auto& snap = *result;
    std::cout << "股票: " << snap.symbol << "\n";

    for (auto iter = snap.entries(); iter.has_next(); ) {
        MDEntry e = iter.next();
        double price = FixedPrice{e.price_raw}.to_double();
        int64_t size = Qty{e.size_raw}.whole();

        if (e.is_bid()) {
            std::cout << "  买: " << price << " x " << size << "\n";
        } else if (e.is_offer()) {
            std::cout << "  卖: " << price << " x " << size << "\n";
        }
    }
}
```

---

## 7. 接收行情更新

```cpp
void on_update(std::span<const char> data) {
    auto result = MarketDataIncrementalRefresh::from_buffer(data);
    if (!result) return;

    for (auto iter = result->entries(); iter.has_next(); ) {
        MDEntry e = iter.next();

        switch (e.update_action) {
            case MDUpdateAction::New:    // 新增
            case MDUpdateAction::Change: // 变更
            case MDUpdateAction::Delete: // 删除
                update_orderbook(e);
                break;
        }
    }
}
```

---

## 8. 消息路由

```cpp
void on_message(std::span<const char> data) {
    auto parser = IndexedParser::parse(data);
    if (!parser) return;

    switch (parser->msg_type()) {
        case '8': on_execution(data);  break;  // 成交回报
        case '9': on_cancel_reject(data); break;  // 撤单拒绝
        case 'W': on_snapshot(data);   break;  // 行情快照
        case 'X': on_update(data);     break;  // 行情更新
        case 'Y': on_md_reject(data);  break;  // 订阅拒绝
    }
}
```

---

## 常用枚举

### 买卖方向 (Side)

| 值 | 含义 |
|----|------|
| `Side::Buy` | 买入 |
| `Side::Sell` | 卖出 |
| `Side::SellShort` | 卖空 |

### 订单类型 (OrdType)

| 值 | 含义 |
|----|------|
| `OrdType::Market` | 市价单 |
| `OrdType::Limit` | 限价单 |
| `OrdType::Stop` | 止损单 |
| `OrdType::StopLimit` | 止损限价单 |

### 订单状态 (OrdStatus)

| 值 | 含义 |
|----|------|
| `OrdStatus::New` | 新订单 |
| `OrdStatus::PartiallyFilled` | 部分成交 |
| `OrdStatus::Filled` | 全部成交 |
| `OrdStatus::Canceled` | 已撤销 |
| `OrdStatus::Rejected` | 已拒绝 |

### 行情类型 (MDEntryType)

| 值 | 含义 |
|----|------|
| `MDEntryType::Bid` | 买盘 |
| `MDEntryType::Offer` | 卖盘 |
| `MDEntryType::Trade` | 成交 |
| `MDEntryType::OpeningPrice` | 开盘价 |
| `MDEntryType::ClosingPrice` | 收盘价 |

---

## 性能指标

| 操作 | 延迟 |
|------|------|
| 解析 ExecutionReport | 246 ns |
| 解析 NewOrderSingle | 229 ns |
| 字段访问 | 11 ns |
| 吞吐量 | 417万 条/秒 |

**比 QuickFIX 快 3 倍**

---

## 消息类型速查

| 类型 | 名称 | 方向 | 用途 |
|------|------|------|------|
| D | NewOrderSingle | 发送 | 下单 |
| F | OrderCancelRequest | 发送 | 撤单 |
| 8 | ExecutionReport | 接收 | 成交回报 |
| V | MarketDataRequest | 发送 | 订阅行情 |
| W | MarketDataSnapshotFullRefresh | 接收 | 行情快照 |
| X | MarketDataIncrementalRefresh | 接收 | 行情更新 |
