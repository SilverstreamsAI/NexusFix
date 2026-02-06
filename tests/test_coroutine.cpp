#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <chrono>
#include <string>
#include <vector>

#include "nexusfix/session/async_primitives.hpp"
#include "nexusfix/session/coroutine.hpp"
#include "nexusfix/session/coroutine_session.hpp"
#include "nexusfix/transport/async_transport.hpp"
#include "nexusfix/transport/socket.hpp"

using namespace nfx;

// ============================================================================
// Coroutine helper functions (avoid lambda capture + coroutine pitfall)
// ============================================================================

static Task<int> lock_and_return(AsyncMutex& mutex, int value) {
    auto lock = co_await mutex.scoped_lock();
    co_return value;
}

static Task<void> lock_and_push(AsyncMutex& mutex, std::vector<int>& order, int id) {
    auto lock = co_await mutex.scoped_lock();
    order.push_back(id);
}

static Task<void> lock_and_set(AsyncMutex& mutex, int& value, int new_val) {
    auto lock = co_await mutex.scoped_lock();
    value = new_val;
}

static Task<int> wait_event_return(Event& event, int value) {
    co_await event;
    co_return value;
}

static Task<void> wait_event_set_flag(Event& event, bool& reached) {
    co_await event;
    reached = true;
}

static Task<void> set_value_task(int& value, int new_val) {
    value = new_val;
    co_return;
}

static Task<void> push_value_task(std::vector<int>& completed, int id) {
    completed.push_back(id);
    co_return;
}

static Task<void> yield_forever() {
    while (true) {
        co_await Yield{};
    }
}

static Task<void> yield_once_task() {
    co_await Yield{};
}

static Task<void> immediate_task() {
    co_return;
}

static Task<int> return_value_task(int v) {
    co_return v;
}

static Task<void> set_flag_task(bool& flag) {
    flag = true;
    co_return;
}

// ============================================================================
// AsyncMutex Tests
// ============================================================================

TEST_CASE("AsyncMutex: single lock/unlock", "[async_mutex]") {
    AsyncMutex mutex;
    auto task = lock_and_return(mutex, 42);
    REQUIRE(task.get() == 42);
}

TEST_CASE("AsyncMutex: ScopedLock RAII releases on destruction", "[async_mutex]") {
    AsyncMutex mutex;
    int value = 0;

    auto task1 = lock_and_set(mutex, value, 1);
    task1.get();
    REQUIRE(value == 1);

    // Should be able to acquire again after RAII release
    auto task2 = lock_and_set(mutex, value, 2);
    task2.get();
    REQUIRE(value == 2);
}

TEST_CASE("AsyncMutex: sequential tasks access shared state", "[async_mutex]") {
    AsyncMutex mutex;
    std::vector<int> order;

    auto t1 = lock_and_push(mutex, order, 1);
    auto t2 = lock_and_push(mutex, order, 2);
    auto t3 = lock_and_push(mutex, order, 3);

    t1.get();
    t2.get();
    t3.get();

    REQUIRE(order.size() == 3);
    REQUIRE(std::find(order.begin(), order.end(), 1) != order.end());
    REQUIRE(std::find(order.begin(), order.end(), 2) != order.end());
    REQUIRE(std::find(order.begin(), order.end(), 3) != order.end());
}

// ============================================================================
// Event Tests
// ============================================================================

TEST_CASE("Event: wait after set returns immediately", "[event]") {
    Event event;
    event.set();

    auto task = wait_event_return(event, 1);
    REQUIRE(task.get() == 1);
}

TEST_CASE("Event: is_set reflects state", "[event]") {
    Event event;
    REQUIRE_FALSE(event.is_set());

    event.set();
    REQUIRE(event.is_set());

    event.reset();
    REQUIRE_FALSE(event.is_set());
}

TEST_CASE("Event: set resumes waiter", "[event]") {
    Event event;
    bool reached = false;

    auto waiter = wait_event_set_flag(event, reached);

    // Start the coroutine - it will suspend at co_await event
    waiter.resume();
    REQUIRE_FALSE(reached);

    // Set the event - should resume the waiter
    event.set();
    REQUIRE(reached);
}

TEST_CASE("Event: reset and re-set", "[event]") {
    Event event;

    event.set();
    REQUIRE(event.is_set());

    event.reset();
    REQUIRE_FALSE(event.is_set());

    event.set();
    REQUIRE(event.is_set());

    auto task = wait_event_return(event, 99);
    REQUIRE(task.get() == 99);
}

// ============================================================================
// WhenAll Tests
// ============================================================================

TEST_CASE("WhenAll: empty vector completes immediately", "[when_all]") {
    auto task = when_all({});
    task.get();
    REQUIRE(true);
}

TEST_CASE("WhenAll: single task", "[when_all]") {
    int value = 0;

    std::vector<Task<void>> tasks;
    tasks.push_back(set_value_task(value, 42));

    auto task = when_all(std::move(tasks));
    task.get();
    REQUIRE(value == 42);
}

TEST_CASE("WhenAll: multiple tasks all complete", "[when_all]") {
    std::vector<int> completed;

    std::vector<Task<void>> tasks;
    tasks.push_back(push_value_task(completed, 1));
    tasks.push_back(push_value_task(completed, 2));
    tasks.push_back(push_value_task(completed, 3));

    auto task = when_all(std::move(tasks));
    task.get();

    REQUIRE(completed.size() == 3);
}

// ============================================================================
// WhenAny Tests
// ============================================================================

TEST_CASE("WhenAny: first completer wins", "[when_any]") {
    std::vector<Task<void>> tasks;
    tasks.push_back(immediate_task());
    tasks.push_back(yield_forever());

    auto task = when_any(std::move(tasks));
    size_t winner = task.get();
    REQUIRE(winner == 0);
}

TEST_CASE("WhenAny: index correctness", "[when_any]") {
    std::vector<Task<void>> tasks;
    tasks.push_back(yield_once_task());
    tasks.push_back(immediate_task());

    auto task = when_any(std::move(tasks));
    size_t winner = task.get();
    // Either index is valid in cooperative scheduling
    REQUIRE(winner < 2);
}

// ============================================================================
// Timeout Tests
// ============================================================================

TEST_CASE("with_timeout: operation completes before timeout", "[timeout]") {
    auto task = with_timeout(return_value_task(42), std::chrono::milliseconds{1000});
    auto result = task.get();

    REQUIRE(result.has_value());
    REQUIRE(*result == 42);
}

TEST_CASE("with_timeout void: operation completes before timeout", "[timeout]") {
    bool completed = false;
    auto task = with_timeout(set_flag_task(completed), std::chrono::milliseconds{1000});
    bool result = task.get();

    REQUIRE(result == true);
    REQUIRE(completed == true);
}

// ============================================================================
// CoroutineSession Tests
// ============================================================================

namespace {

/// Mock transport for testing
class MockTransport : public ITransport {
public:
    std::vector<std::vector<char>> recv_queue;
    size_t recv_index{0};
    std::vector<std::vector<char>> sent_data;
    bool connected{false};
    bool connect_should_fail{false};

    [[nodiscard]] TransportResult<void> connect(
        std::string_view, uint16_t) override {
        if (connect_should_fail) {
            return std::unexpected{TransportError{TransportErrorCode::ConnectionRefused}};
        }
        connected = true;
        return {};
    }

    void disconnect() override { connected = false; }
    [[nodiscard]] bool is_connected() const override { return connected; }

    [[nodiscard]] TransportResult<size_t> send(
        std::span<const char> data) override {
        if (!connected) {
            return std::unexpected{TransportError{TransportErrorCode::NotConnected}};
        }
        sent_data.emplace_back(data.begin(), data.end());
        return data.size();
    }

    [[nodiscard]] TransportResult<size_t> receive(
        std::span<char> buffer) override {
        if (!connected) {
            return std::unexpected{TransportError{TransportErrorCode::NotConnected}};
        }
        if (recv_index >= recv_queue.size()) {
            return size_t{0};
        }
        const auto& msg = recv_queue[recv_index++];
        size_t to_copy = std::min(msg.size(), buffer.size());
        std::copy_n(msg.begin(), to_copy, buffer.begin());
        return to_copy;
    }

    [[nodiscard]] bool set_nodelay(bool) override { return true; }
    [[nodiscard]] bool set_keepalive(bool) override { return true; }
    [[nodiscard]] bool set_receive_timeout(int) override { return true; }
    [[nodiscard]] bool set_send_timeout(int) override { return true; }

    void queue_logon_response() {
        std::string logon_resp =
            "8=FIX.4.4\x01" "9=63\x01" "35=A\x01" "49=TARGET\x01" "56=SENDER\x01"
            "34=1\x01" "52=20231215-10:30:00\x01" "98=0\x01" "108=30\x01" "10=173\x01";
        recv_queue.emplace_back(logon_resp.begin(), logon_resp.end());
    }

    void queue_logout_response() {
        std::string logout_resp =
            "8=FIX.4.4\x01" "9=51\x01" "35=5\x01" "49=TARGET\x01" "56=SENDER\x01"
            "34=2\x01" "52=20231215-10:30:01\x01" "10=135\x01";
        recv_queue.emplace_back(logout_resp.begin(), logout_resp.end());
    }
};

/// Test handler that records events
struct TestHandler {
    bool logon_called{false};
    bool logout_called{false};
    std::string_view logout_reason;
    std::vector<std::pair<SessionState, SessionState>> state_changes;
    std::vector<SessionError> errors;
    int app_messages{0};

    void on_app_message(const ParsedMessage&) noexcept { ++app_messages; }
    void on_state_change(SessionState from, SessionState to) noexcept {
        state_changes.emplace_back(from, to);
    }
    bool on_send(std::span<const char>) noexcept { return true; }
    void on_error(const SessionError& err) noexcept { errors.push_back(err); }
    void on_logon() noexcept { logon_called = true; }
    void on_logout(std::string_view reason) noexcept {
        logout_called = true;
        logout_reason = reason;
    }
};

static_assert(SessionHandler<TestHandler>,
    "TestHandler must satisfy SessionHandler concept");

} // namespace

TEST_CASE("CoroutineSession: connect failure returns error", "[coroutine_session]") {
    SessionConfig config;
    config.sender_comp_id = "SENDER";
    config.target_comp_id = "TARGET";
    config.logon_timeout = 1;

    TestHandler handler;
    MockTransport mock_transport;
    mock_transport.connect_should_fail = true;

    AsyncTransport async_transport{mock_transport};
    CoroutineSession<TestHandler> session{config, handler, async_transport};

    auto task = session.run("localhost", 9876);
    auto result = task.get();

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == SessionErrorCode::NotConnected);
}

TEST_CASE("CoroutineSession: successful connect transitions state", "[coroutine_session]") {
    SessionConfig config;
    config.sender_comp_id = "SENDER";
    config.target_comp_id = "TARGET";
    config.logon_timeout = 1;

    TestHandler handler;
    MockTransport mock_transport;
    mock_transport.queue_logon_response();
    mock_transport.queue_logout_response();

    AsyncTransport async_transport{mock_transport};
    CoroutineSession<TestHandler> session{config, handler, async_transport};

    // Request shutdown immediately so active phase exits
    session.request_shutdown();

    auto task = session.run("localhost", 9876);
    auto result = task.get();

    // Verify state transitions include connect
    REQUIRE_FALSE(handler.state_changes.empty());
    REQUIRE(handler.state_changes[0].first == SessionState::Disconnected);
    REQUIRE(handler.state_changes[0].second == SessionState::SocketConnected);

    // Check if logon was attempted (at least LogonSent transition)
    bool logon_sent = false;
    for (const auto& [from, to] : handler.state_changes) {
        if (to == SessionState::LogonSent) logon_sent = true;
    }
    REQUIRE(logon_sent);
}

TEST_CASE("CoroutineSession: connect and send logon", "[coroutine_session]") {
    SessionConfig config;
    config.sender_comp_id = "SENDER";
    config.target_comp_id = "TARGET";
    config.logon_timeout = 5;

    TestHandler handler;
    MockTransport mock_transport;
    mock_transport.queue_logon_response();

    AsyncTransport async_transport{mock_transport};
    CoroutineSession<TestHandler> session{config, handler, async_transport};

    // Just run connect phase and check logon is sent
    session.request_shutdown();

    auto task = session.run("localhost", 9876);
    auto result = task.get();

    // Check that at least a logon message was sent to transport
    bool logon_msg_sent = false;
    for (const auto& sent : mock_transport.sent_data) {
        std::string_view sv(sent.data(), sent.size());
        if (sv.find("35=A") != std::string_view::npos) {
            logon_msg_sent = true;
        }
    }
    REQUIRE(logon_msg_sent);

    // Check that the logon response was received by looking at recv_index
    REQUIRE(mock_transport.recv_index >= 1);
}

TEST_CASE("CoroutineSession: logon and shutdown lifecycle", "[coroutine_session]") {
    SessionConfig config;
    config.sender_comp_id = "SENDER";
    config.target_comp_id = "TARGET";
    config.logon_timeout = 5;
    config.logout_timeout = 2;

    TestHandler handler;
    MockTransport mock_transport;
    mock_transport.queue_logon_response();
    mock_transport.queue_logout_response();

    AsyncTransport async_transport{mock_transport};
    CoroutineSession<TestHandler> session{config, handler, async_transport};

    // Request shutdown so session doesn't run forever
    session.request_shutdown();

    auto task = session.run("localhost", 9876);
    auto result = task.get();

    // Verify state transitions
    REQUIRE(handler.state_changes.size() >= 2);
    REQUIRE(handler.state_changes[0].second == SessionState::SocketConnected);
    REQUIRE(handler.state_changes[1].second == SessionState::LogonSent);

    // Check if logon response was received
    bool logon_received = false;
    for (const auto& [from, to] : handler.state_changes) {
        if (to == SessionState::Active) logon_received = true;
    }

    // With mock transport providing immediate responses, logon should succeed
    REQUIRE(logon_received);
    REQUIRE(handler.logon_called);
    REQUIRE(handler.logout_called);
    REQUIRE_FALSE(mock_transport.sent_data.empty());
}
