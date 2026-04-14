#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vector>
#include <chrono>
#include <string>
#include <deque>
#include <mutex>
#include <optional>

#include "../traders/oracle_service.h"

using namespace simulation;

// Minimal mock to simulate the Websocket client and inject oracle data for testing
class MockWebsocketClient : public transport::WebsocketManagerClient {
public:
    MockWebsocketClient(const std::string& logger_name = "mock_ws_logger") 
        : transport::WebsocketManagerClient(logger_name) {}

    virtual std::optional<std::string> dequeue_message(int connection_id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_messages.empty()) return std::nullopt;
        std::string msg = m_messages.front();
        m_messages.pop_front();
        return msg;
    }

    void inject_message(const std::string& msg) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_messages.push_back(msg);
    }

private:
    std::mutex m_mutex;
    std::deque<std::string> m_messages;
};

TEST(OracleTest, ClientParsesPriceUpdates) {
    auto mock_ws = std::make_shared<MockWebsocketClient>();
    
    // Inject a fundamental price update
    mock_ws->inject_message(R"({
        "ticker": "AAPL",
        "true_price": 155.50,
        "timestamp": 1690000000
    })");

    OracleClient client(mock_ws);
    client.start();

    // Give the client thread a moment to dequeue and parse
    int retries = 0;
    while (client.get_true_price("AAPL") == 0.0 && retries < 50) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        retries++;
    }

    // Verify the price was correctly parsed and stored
    EXPECT_DOUBLE_EQ(client.get_true_price("AAPL"), 155.50);
    EXPECT_DOUBLE_EQ(client.get_true_price("MSFT"), 0.0); // Never injected

    client.stop();
}

TEST(OracleTest, ServiceLifecycleAndGBM) {
    std::vector<std::string> tickers = {"AAPL", "MSFT"};
    std::map<std::string, double> init_prices = {
        {"AAPL", 150.0},
        {"MSFT", 300.0}
    };
    
    OracleConfig config;
    config.update_interval_ms = 5; // Fast tick for testing
    config.mu = 0.0;
    config.sigma = 0.5; // High volatility

    // Instantiate without a real websocket server (nullptr) just to test the math loop
    OracleService service(tickers, init_prices, nullptr, config);
    
    service.start();

    // Let the GBM and Jump Diffusion math run for a few ticks
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    service.stop();

    // If it doesn't crash or segfault, the math loop handles itself safely.
    SUCCEED();
}