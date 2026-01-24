// simple_client.cpp
// NexusFIX Example FIX Initiator (Client)
// Demonstrates basic session management and order flow

#include <iostream>
#include <iomanip>
#include <string>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>

#include "nexusfix/nexusfix.hpp"

namespace {

// Global flag for graceful shutdown
std::atomic<bool> g_running{true};

void signal_handler(int /*sig*/) {
    g_running = false;
}

// ============================================================================
// Utility Functions
// ============================================================================

/// Get current timestamp in FIX format (YYYYMMDD-HH:MM:SS.sss)
std::string get_sending_time() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm{};
    gmtime_r(&time_t, &tm);

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d%02d%02d-%02d:%02d:%02d.%03d",
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec,
        static_cast<int>(ms.count()));

    return buf;
}

/// Generate unique ClOrdID
std::string generate_cl_ord_id() {
    static std::atomic<uint64_t> counter{0};
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    char buf[32];
    std::snprintf(buf, sizeof(buf), "ORD%lu%lu",
        static_cast<unsigned long>(ms),
        static_cast<unsigned long>(counter++));
    return buf;
}

// ============================================================================
// Simple FIX Client
// ============================================================================

class SimpleFixClient {
public:
    struct Config {
        std::string host = "localhost";
        uint16_t port = 9876;
        std::string sender_comp_id = "CLIENT";
        std::string target_comp_id = "SERVER";
        int heartbeat_interval = 30;
    };

    explicit SimpleFixClient(const Config& config)
        : config_{config}
        , seq_mgr_{}
        , assembler_{}
        , state_{nfx::SessionState::Disconnected}
    {}

    /// Connect and initiate session
    bool connect() {
        std::cout << "[INFO] Connecting to " << config_.host
                  << ":" << config_.port << "...\n";

        auto result = transport_.connect(config_.host, config_.port);
        if (!result) {
            std::cerr << "[ERROR] Connection failed\n";
            return false;
        }

        std::cout << "[INFO] Connected, sending Logon...\n";
        state_ = nfx::SessionState::LogonSent;

        // Send Logon message
        if (!send_logon()) {
            std::cerr << "[ERROR] Failed to send Logon\n";
            return false;
        }

        return true;
    }

    /// Disconnect and terminate session
    void disconnect() {
        if (state_ == nfx::SessionState::Active) {
            send_logout("Normal termination");
        }
        transport_.disconnect();
        state_ = nfx::SessionState::Disconnected;
        std::cout << "[INFO] Disconnected\n";
    }

    /// Main event loop
    void run() {
        std::array<char, 4096> buffer;
        std::string recv_buffer;
        recv_buffer.reserve(8192);

        while (g_running && transport_.is_connected()) {
            // Receive data
            auto recv_result = transport_.receive(std::span<char>{buffer});
            if (!recv_result) {
                if (recv_result.error().code == nfx::TransportErrorCode::ConnectionClosed) {
                    std::cout << "[INFO] Connection closed by peer\n";
                    break;
                }
                // Timeout or would-block, continue
                check_heartbeat();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            size_t bytes_received = *recv_result;
            if (bytes_received == 0) {
                check_heartbeat();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // Append to receive buffer
            recv_buffer.append(buffer.data(), bytes_received);

            // Process complete messages
            process_buffer(recv_buffer);

            check_heartbeat();
        }
    }

    /// Send a new order
    bool send_new_order(
        const std::string& symbol,
        nfx::Side side,
        double qty,
        nfx::OrdType ord_type,
        double price = 0.0)
    {
        if (state_ != nfx::SessionState::Active) {
            std::cerr << "[ERROR] Session not active\n";
            return false;
        }

        std::string cl_ord_id = generate_cl_ord_id();
        std::string sending_time = get_sending_time();
        uint32_t seq_num = seq_mgr_.next_outbound();

        nfx::fix44::NewOrderSingle::Builder builder;

        builder
            .sender_comp_id(config_.sender_comp_id)
            .target_comp_id(config_.target_comp_id)
            .msg_seq_num(seq_num)
            .sending_time(sending_time)
            .cl_ord_id(cl_ord_id)
            .symbol(symbol)
            .side(side)
            .transact_time(sending_time)
            .order_qty(nfx::Qty{static_cast<int64_t>(qty)})
            .ord_type(ord_type);

        if (ord_type == nfx::OrdType::Limit && price > 0) {
            builder.price(nfx::FixedPrice::from_double(price));
        }

        builder.time_in_force(nfx::TimeInForce::Day);

        auto msg = builder.build(assembler_);

        std::cout << "[ORDER] Sending NewOrderSingle: " << cl_ord_id
                  << " " << symbol << " "
                  << (side == nfx::Side::Buy ? "BUY" : "SELL")
                  << " " << qty
                  << (ord_type == nfx::OrdType::Limit ? " @ " + std::to_string(price) : " MKT")
                  << "\n";

        return send_message(msg);
    }

    /// Send order cancel request
    bool send_cancel_order(
        const std::string& orig_cl_ord_id,
        const std::string& symbol,
        nfx::Side side)
    {
        if (state_ != nfx::SessionState::Active) {
            std::cerr << "[ERROR] Session not active\n";
            return false;
        }

        std::string cl_ord_id = generate_cl_ord_id();
        std::string sending_time = get_sending_time();
        uint32_t seq_num = seq_mgr_.next_outbound();

        nfx::fix44::OrderCancelRequest::Builder builder;

        auto msg = builder
            .sender_comp_id(config_.sender_comp_id)
            .target_comp_id(config_.target_comp_id)
            .msg_seq_num(seq_num)
            .sending_time(sending_time)
            .orig_cl_ord_id(orig_cl_ord_id)
            .cl_ord_id(cl_ord_id)
            .symbol(symbol)
            .side(side)
            .transact_time(sending_time)
            .build(assembler_);

        std::cout << "[CANCEL] Sending OrderCancelRequest for: " << orig_cl_ord_id << "\n";

        return send_message(msg);
    }

    bool is_active() const { return state_ == nfx::SessionState::Active; }

private:
    /// Process buffer looking for complete messages
    void process_buffer(std::string& buffer) {
        while (!buffer.empty()) {
            std::span<const char> data{buffer.data(), buffer.size()};

            // Try to find message boundary
            auto boundary = nfx::simd::find_message_boundary(data);
            if (!boundary.complete) {
                break;  // Need more data
            }

            // Extract and process message
            auto msg_data = data.subspan(boundary.start, boundary.end - boundary.start);
            process_message(msg_data);

            // Remove processed message from buffer
            buffer.erase(0, boundary.end);
        }
    }

    /// Send Logon message
    bool send_logon() {
        uint32_t seq_num = seq_mgr_.next_outbound();

        nfx::fix44::Logon::Builder builder;
        auto msg = builder
            .sender_comp_id(config_.sender_comp_id)
            .target_comp_id(config_.target_comp_id)
            .msg_seq_num(seq_num)
            .sending_time(get_sending_time())
            .encrypt_method(0)  // None
            .heart_bt_int(config_.heartbeat_interval)
            .build(assembler_);

        return send_message(msg);
    }

    /// Send Logout message
    bool send_logout(const std::string& text) {
        uint32_t seq_num = seq_mgr_.next_outbound();

        nfx::fix44::Logout::Builder builder;
        auto msg = builder
            .sender_comp_id(config_.sender_comp_id)
            .target_comp_id(config_.target_comp_id)
            .msg_seq_num(seq_num)
            .sending_time(get_sending_time())
            .text(text)
            .build(assembler_);

        state_ = nfx::SessionState::LogoutPending;
        return send_message(msg);
    }

    /// Send Heartbeat message
    bool send_heartbeat(const std::string& test_req_id = "") {
        uint32_t seq_num = seq_mgr_.next_outbound();

        nfx::fix44::Heartbeat::Builder builder;
        builder
            .sender_comp_id(config_.sender_comp_id)
            .target_comp_id(config_.target_comp_id)
            .msg_seq_num(seq_num)
            .sending_time(get_sending_time());

        if (!test_req_id.empty()) {
            builder.test_req_id(test_req_id);
        }

        auto msg = builder.build(assembler_);
        return send_message(msg);
    }

    /// Send raw message
    bool send_message(std::span<const char> msg) {
        auto result = transport_.send(msg);
        if (!result) {
            std::cerr << "[ERROR] Failed to send message\n";
            return false;
        }
        last_sent_time_ = std::chrono::steady_clock::now();
        return true;
    }

    /// Process received message
    void process_message(std::span<const char> msg) {
        auto result = nfx::IndexedParser::parse(msg);
        if (!result) {
            std::cerr << "[ERROR] Failed to parse message\n";
            return;
        }

        const auto& parsed = *result;
        last_recv_time_ = std::chrono::steady_clock::now();

        // Validate sequence number
        uint32_t seq = parsed.msg_seq_num();
        auto validation = seq_mgr_.validate_inbound(seq);
        if (validation == nfx::SequenceManager::SequenceResult::GapDetected) {
            std::cout << "[WARN] Sequence gap detected at " << seq << "\n";
        } else if (validation == nfx::SequenceManager::SequenceResult::TooLow) {
            std::cout << "[WARN] Duplicate message " << seq << "\n";
        }

        // Get message type
        char msg_type = parsed.msg_type();

        switch (msg_type) {
            case 'A':  // Logon
                handle_logon(parsed);
                break;
            case '0':  // Heartbeat
                handle_heartbeat(parsed);
                break;
            case '1':  // TestRequest
                handle_test_request(parsed);
                break;
            case '5':  // Logout
                handle_logout(parsed);
                break;
            case '8':  // ExecutionReport
                handle_execution_report(parsed);
                break;
            case '9':  // OrderCancelReject
                handle_order_cancel_reject(parsed);
                break;
            case '3':  // Reject
                handle_reject(parsed);
                break;
            default:
                std::cout << "[INFO] Received message type: " << msg_type << "\n";
                break;
        }
    }

    void handle_logon(const nfx::IndexedParser& msg) {
        std::cout << "[SESSION] Logon received, session active\n";
        state_ = nfx::SessionState::Active;

        auto hb_int = msg.get_int(108);  // HeartBtInt
        if (hb_int) {
            heartbeat_interval_ = std::chrono::seconds(*hb_int);
            std::cout << "[SESSION] Heartbeat interval: "
                      << *hb_int << " seconds\n";
        }
    }

    void handle_heartbeat(const nfx::IndexedParser& /*msg*/) {
        std::cout << "[HEARTBEAT] Received\n";
    }

    void handle_test_request(const nfx::IndexedParser& msg) {
        std::cout << "[TEST_REQUEST] Received\n";
        std::string_view test_req_id = msg.get_string(112);  // TestReqID
        if (!test_req_id.empty()) {
            send_heartbeat(std::string{test_req_id});
        }
    }

    void handle_logout(const nfx::IndexedParser& msg) {
        std::cout << "[SESSION] Logout received\n";
        std::string_view text = msg.get_string(58);
        if (!text.empty()) {
            std::cout << "[SESSION] Reason: " << text << "\n";
        }
        state_ = nfx::SessionState::Disconnected;
    }

    void handle_execution_report(const nfx::IndexedParser& msg) {
        std::string_view order_id = msg.get_string(37);
        std::string_view exec_id = msg.get_string(17);
        char exec_type = msg.get_char(150);
        char ord_status = msg.get_char(39);
        std::string_view symbol = msg.get_string(55);
        char side = msg.get_char(54);
        auto leaves_qty = msg.get_int(151);
        auto cum_qty = msg.get_int(14);
        std::string_view avg_px_str = msg.get_string(6);

        std::cout << "[EXEC_REPORT] ";
        if (!order_id.empty()) std::cout << "OrderID=" << order_id << " ";
        if (!exec_id.empty()) std::cout << "ExecID=" << exec_id << " ";
        if (exec_type != '\0') {
            std::cout << "ExecType=" << exec_type << "(";
            switch (exec_type) {
                case '0': std::cout << "New"; break;
                case '1': std::cout << "PartialFill"; break;
                case '2': std::cout << "Fill"; break;
                case '4': std::cout << "Canceled"; break;
                case '8': std::cout << "Rejected"; break;
                default: std::cout << "Other"; break;
            }
            std::cout << ") ";
        }
        if (!symbol.empty()) std::cout << "Symbol=" << symbol << " ";
        if (side != '\0') std::cout << "Side=" << (side == '1' ? "Buy" : "Sell") << " ";
        if (leaves_qty) std::cout << "LeavesQty=" << *leaves_qty << " ";
        if (cum_qty) std::cout << "CumQty=" << *cum_qty << " ";
        if (!avg_px_str.empty()) std::cout << "AvgPx=" << avg_px_str << " ";
        std::cout << "\n";
        (void)ord_status;
    }

    void handle_order_cancel_reject(const nfx::IndexedParser& msg) {
        std::string_view order_id = msg.get_string(37);
        std::string_view cl_ord_id = msg.get_string(11);
        std::string_view orig_cl_ord_id = msg.get_string(41);
        std::string_view text = msg.get_string(58);

        std::cout << "[CANCEL_REJECT] ";
        if (!order_id.empty()) std::cout << "OrderID=" << order_id << " ";
        if (!cl_ord_id.empty()) std::cout << "ClOrdID=" << cl_ord_id << " ";
        if (!orig_cl_ord_id.empty()) std::cout << "OrigClOrdID=" << orig_cl_ord_id << " ";
        if (!text.empty()) std::cout << "Text=" << text << " ";
        std::cout << "\n";
    }

    void handle_reject(const nfx::IndexedParser& msg) {
        auto ref_seq_num = msg.get_int(45);
        std::string_view text = msg.get_string(58);
        auto reason = msg.get_int(373);

        std::cout << "[REJECT] ";
        if (ref_seq_num) std::cout << "RefSeqNum=" << *ref_seq_num << " ";
        if (reason) std::cout << "Reason=" << *reason << " ";
        if (!text.empty()) std::cout << "Text=" << text << " ";
        std::cout << "\n";
    }

    /// Check if heartbeat is needed
    void check_heartbeat() {
        if (state_ != nfx::SessionState::Active) return;

        auto now = std::chrono::steady_clock::now();
        auto since_last_sent = std::chrono::duration_cast<std::chrono::seconds>(
            now - last_sent_time_);

        if (since_last_sent >= heartbeat_interval_) {
            std::cout << "[HEARTBEAT] Sending...\n";
            send_heartbeat();
        }
    }

    Config config_;
    nfx::TcpTransport transport_;
    nfx::SequenceManager seq_mgr_;
    nfx::MessageAssembler assembler_;
    nfx::SessionState state_;

    std::chrono::steady_clock::time_point last_sent_time_;
    std::chrono::steady_clock::time_point last_recv_time_;
    std::chrono::seconds heartbeat_interval_{30};
};

} // anonymous namespace

// ============================================================================
// Main
// ============================================================================

void print_usage(const char* program) {
    std::cout << "Usage: " << program << " [options]\n"
              << "Options:\n"
              << "  -h, --host HOST      Server hostname (default: localhost)\n"
              << "  -p, --port PORT      Server port (default: 9876)\n"
              << "  -s, --sender ID      SenderCompID (default: CLIENT)\n"
              << "  -t, --target ID      TargetCompID (default: SERVER)\n"
              << "  --help               Show this help\n";
}

int main(int argc, char* argv[]) {
    SimpleFixClient::Config config;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-h" || arg == "--host") && i + 1 < argc) {
            config.host = argv[++i];
        } else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            config.port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if ((arg == "-s" || arg == "--sender") && i + 1 < argc) {
            config.sender_comp_id = argv[++i];
        } else if ((arg == "-t" || arg == "--target") && i + 1 < argc) {
            config.target_comp_id = argv[++i];
        } else if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
    }

    // Setup signal handler for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "NexusFIX Simple Client Example\n";
    std::cout << "==============================\n";
    std::cout << "Host: " << config.host << ":" << config.port << "\n";
    std::cout << "SenderCompID: " << config.sender_comp_id << "\n";
    std::cout << "TargetCompID: " << config.target_comp_id << "\n";
    std::cout << "\nPress Ctrl+C to exit\n\n";

    SimpleFixClient client(config);

    if (!client.connect()) {
        std::cerr << "Failed to connect\n";
        return 1;
    }

    // Wait for session to become active
    int attempts = 0;
    while (!client.is_active() && attempts < 50 && g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ++attempts;
    }

    if (!client.is_active()) {
        std::cerr << "Session failed to activate\n";
        client.disconnect();
        return 1;
    }

    // Demo: Send some orders
    std::cout << "\n[DEMO] Sending test orders...\n\n";

    // Buy 100 AAPL @ 150.00
    client.send_new_order("AAPL", nfx::Side::Buy, 100, nfx::OrdType::Limit, 150.00);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Sell 50 MSFT @ Market
    client.send_new_order("MSFT", nfx::Side::Sell, 50, nfx::OrdType::Market);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Buy 200 GOOGL @ 140.50
    client.send_new_order("GOOGL", nfx::Side::Buy, 200, nfx::OrdType::Limit, 140.50);

    // Run main loop
    client.run();

    // Cleanup
    client.disconnect();

    std::cout << "\nClient terminated.\n";
    return 0;
}
