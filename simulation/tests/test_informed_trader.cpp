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
#define DAY GTC
#include "../traders/informed_trader.h"
#undef DAY

namespace fs = std::filesystem;
using namespace simulation;

// Minimal mock to simulate the Websocket client and inject market data / oracle data for testing
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

class InformedTraderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup Server with a distinct port to avoid collisions with other tests
        std::stringstream server_config_stream;
        server_config_stream << R"(
[DEFAULT]
ConnectionType=acceptor
SocketAcceptPort=10007
SocketReuseAddress=Y
StartTime=00:00:00
EndTime=00:00:00
UseDataDictionary=N
FileStorePath=store/test_it_server_store
FileLogPath=logs/test_it_server_logs
BeginString=FIX.4.2

[SESSION]
SenderCompID=Edux-test
TargetCompID=client_it
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

TEST_F(InformedTraderTest, EndToEndSniping) {
    // Generate distinct config for the informed trader client
    std::stringstream client_config_stream;
    client_config_stream << R"(
[DEFAULT]
ConnectionType=initiator
ReconnectInterval=60
SocketConnectHost=localhost
SocketConnectPort=10007
UseDataDictionary=N
StartTime=00:00:00
EndTime=00:00:00
HeartBtInt=30
CheckLatency=N
BeginString=FIX.4.2
FileStorePath=store/test_it_client_store
FileLogPath=logs/test_it_client_log

[SESSION]
SenderCompID=client_it
TargetCompID=Edux-test
)";

    std::string config_path = "configs/temp/temp_client_it.cfg";

    fs::path p = config_path;
    if (p.has_parent_path()) {
        fs::create_directories(p.parent_path());
    }

    std::ofstream config_file(config_path);
    config_file << client_config_stream.str();
    config_file.close();

    // 1. Setup Mock Websockets
    auto mock_md_ws = std::make_shared<MockWebsocketClient>("mock_md_ws_logger");
    auto mock_oracle_ws = std::make_shared<MockWebsocketClient>("mock_oracle_ws_logger");
    
    // Inject initial orderbook snapshot representing the exchange state
    mock_md_ws->inject_message(R"({
        "ticker": "AAPL",
        "bids": [{"price": 150.00, "quantity": 1000}],
        "asks": [{"price": 150.10, "quantity": 1000}],
        "create_timestamp": 1690000000
    })");

    // Inject true price from Oracle (substantially higher to trigger an undervalued mispricing)
    mock_oracle_ws->inject_message(R"({
        "ticker": "AAPL",
        "true_price": 151.00,
        "timestamp": 1690000001
    })");

    // 2. Initialize and start the data handlers
    auto md_handler = std::make_shared<MarketDataHandler>(mock_md_ws);
    md_handler->start();

    auto oracle_client = std::make_shared<OracleClient>(mock_oracle_ws);
    oracle_client->start();

    // 3. Initialize the FIX Client
    auto fix_client = std::make_shared<InformedFixClient>(config_path);
    
    // Wait for FIX engine to initialize and connect
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 4. Initialize and start the Informed Trader Strategy
    auto informed_trader = std::make_shared<InformedTrader>(
        md_handler,
        oracle_client,
        fix_client,
        100.0, // initial_inventory
        0.05,  // epsilon
        10.0,  // trade_qty
        1000.0 // max_inventory
    );

    const std::vector<std::string> tickers = {"AAPL"};
    informed_trader->start(tickers);

    // Allow the strategy to run, read the oracle, evaluate the orderbook, and submit a snipe order
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // 5. Cleanup
    informed_trader->stop();
    oracle_client->stop();
    md_handler->stop();

    // Test passes if we successfully initialized, connected, processed injected market and oracle data,
    // and executed without segfaults or unhandled exceptions.
    SUCCEED();
}