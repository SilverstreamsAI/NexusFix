// Compilation test - verifies all headers compile correctly
#include "nexusfix/nexusfix.hpp"

#include <iostream>

int main() {
    using namespace nfx;
    using namespace nfx::literals;

    // Test types
    auto price = 100.50_price;
    auto qty = 100_qty;
    auto seq = 1_seq;

    std::cout << "Price: " << price.to_double() << "\n";
    std::cout << "Qty: " << qty.whole() << "\n";
    std::cout << "SeqNum: " << seq.get() << "\n";

    // Test FIX message parsing
    const char msg[] =
        "8=FIX.4.4\x01" "9=70\x01" "35=A\x01" "49=CLIENT\x01" "56=SERVER\x01"
        "34=1\x01" "52=20231215-10:30:00\x01" "98=0\x01" "108=30\x01" "10=000\x01";

    auto result = parse_message(std::span<const char>{msg, sizeof(msg) - 1});
    if (result.has_value()) {
        std::cout << "Parsed message type: " << result->msg_type() << "\n";
        std::cout << "Sender: " << result->sender_comp_id() << "\n";
        std::cout << "Target: " << result->target_comp_id() << "\n";
        std::cout << "SeqNum: " << result->msg_seq_num() << "\n";
    } else {
        std::cout << "Parse error: " << result.error().message() << "\n";
    }

    // Test memory pool
    MessagePool pool;
    auto buf = pool.allocate(256);
    if (!buf.empty()) {
        std::cout << "Allocated buffer of size: " << buf.size() << "\n";
        pool.deallocate(buf);
    }

    // Test SIMD scanner
    auto soh_count = simd::count_soh(std::span<const char>{msg, sizeof(msg) - 1});
    std::cout << "SOH count: " << soh_count << "\n";

    std::cout << "NexusFIX version: " << VERSION.string() << "\n";
    std::cout << "All tests passed!\n";

    return 0;
}
