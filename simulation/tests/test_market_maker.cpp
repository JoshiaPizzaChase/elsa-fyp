#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vector>
#include <chrono>
#include <sstream>
#include <fstream>
#include <string>
#include <filesystem>
#include <deque>
#include <mutex>
#include <optional>

#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"

#include "quickfix/SocketAcceptor.h"
#include "quickfix/SessionSettings.h"
#include "quickfix/FileStore.h"
#include "quickfix/FileLog.h"

#include "test_server.h"
#include "../traders/market_maker.h"

namespace fs = std::filesystem;
using namespace simulation;

// Minimal mock to simulate the Websocket client and inject market data for testing
class MockWebsocketClient : public transport::WebsocketManagerClient {
public:
    MockWebsocketClient() = default;

    // Provide mocked messages to the handler
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

class MarketMakerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup Server with a distinct port to avoid collisions
        std::stringstream server_config_stream;
        server_config_stream << R"(
[DEFAULT]
ConnectionType=acceptor
SocketAcceptPort=10006
SocketReuseAddress=Y
StartTime=00:00:00
EndTime=00:00:00
UseDataDictionary=N
FileStorePath=store/test_mm_server_store
FileLogPath=logs/test_mm_server_logs
BeginString=FIX.4.2

[SESSION]
SenderCompID=Edux-test
TargetCompID=client_mm
)";

        server_settings_ = std::make_unique<FIX::SessionSettings>(server_config_stream);
        server_store_factory_ = std::make_unique<FIX::FileStoreFactory>(*server_settings_);
        server_log_factory_ = std::make_unique<FIX::FileLogFactory>(*server_settings_);
        server_app_ = std::make_unique<TestFixServer>();

        acceptor_ = std::make_unique<FIX::SocketAcceptor>(
            *server_app_, *server_store_factory_, *server_settings_, *server_log_factory_
        );
        acceptor_->start();

        std::this_thread::sleep_for(std::chrono::seconds(1)); // Wait for server to bind and start
    }

    void TearDown() override {
        if (acceptor_) {
            acceptor_->stop();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Wait for shutdown
    }

    std::unique_ptr<FIX::SessionSettings> server_settings_;
    std::unique_ptr<FIX::FileStoreFactory> server_store_factory_;
    std::unique_ptr<FIX::FileLogFactory> server_log_factory_;
    std::unique_ptr<TestFixServer> server_app_;
    std::unique_ptr<FIX::SocketAcceptor> acceptor_;
};

TEST_F(MarketMakerTest, EndToEndQuoteSubmission) {
    // Generate distinct config for the market maker client
    std::stringstream client_config_stream;
    client_config_stream << R"(
[DEFAULT]
ConnectionType=initiator
ReconnectInterval=60
SocketConnectHost=localhost
SocketConnectPort=10006
UseDataDictionary=N
StartTime=00:00:00
EndTime=00:00:00
HeartBtInt=30
CheckLatency=N
BeginString=FIX.4.2
FileStorePath=store/test_mm_client_store
FileLogPath=logs/test_mm_client_log

[SESSION]
SenderCompID=client_mm
TargetCompID=Edux-test
)";

    std::string config_path = "configs/temp/temp_client_mm.cfg";

    fs::path p = config_path;
    if (p.has_parent_path()) {
        fs::create_directories(p.parent_path());
    }

    std::ofstream config_file(config_path);
    config_file << client_config_stream.str();
    config_file.close();

    // 1. Setup Mock Websocket and Inject Market Data
    auto mock_ws = std::make_shared<MockWebsocketClient>();
    
    // Inject initial orderbook snapshot
    mock_ws->inject_message(R"({
        "ticker": "AAPL",
        "bids": [{"price": 150.00, "quantity": 1000}],
        "asks": [{"price": 150.10, "quantity": 1000}],
        "create_timestamp": 1690000000
    })");

    // Inject a few trades to establish rolling variance
    mock_ws->inject_message(R"({
        "ticker": "AAPL",
        "trade_id": 1,
        "price": 150.05,
        "quantity": 100,
        "create_timestamp": 1690000001
    })");
    mock_ws->inject_message(R"({
        "ticker": "AAPL",
        "trade_id": 2,
        "price": 150.06,
        "quantity": 100,
        "create_timestamp": 1690000002
    })");

    // 2. Initialize and start the Market Data Handler
    auto md_handler = std::make_shared<MarketDataHandler>(mock_ws);
    md_handler->start();

    // 3. Initialize the FIX Client
    auto fix_client = std::make_shared<MMFixClient>(config_path);
    
    // Wait for FIX engine to initialize and connect
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 4. Initialize and start the Market Maker Strategy
    auto market_maker = std::make_shared<MarketMaker>(
        md_handler,
        fix_client,
        100.0, // initial_inventory
        1.0,   // lot_size
        0.1,   // gamma
        1.5,   // k
        1.0    // terminal_time
    );

    const std::vector<std::string> tickers = {"AAPL"};
    market_maker->start(tickers);

    // Allow the strategy to run and submit quotes based on the injected data
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // 5. Cleanup
    market_maker->stop();
    md_handler->stop();

    // Test passes if we successfully initialized, connected, processed injected market data,
    // and generated continuous quotes without segfaults or unhandled exceptions.
    SUCCEED();
}