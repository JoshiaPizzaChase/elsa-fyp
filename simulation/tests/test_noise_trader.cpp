#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vector>
#include <chrono>
#include <sstream>
#include <fstream>
#include <string>

#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"

#include "noise_trader.h"
#include "test_client.h"
#include "test_server.h"
#include "quickfix/SocketAcceptor.h"
#include "quickfix/SessionSettings.h"
#include "quickfix/FileStore.h"
#include "quickfix/FileLog.h"

#include <filesystem>

namespace fs = std::filesystem;

using namespace simulation;

class NoiseTraderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup Server with 5 distinct sessions to avoid session collisions
        std::stringstream server_config_stream;
        server_config_stream << R"(
[DEFAULT]
ConnectionType=acceptor
SocketAcceptPort=10005
SocketReuseAddress=Y
StartTime=00:00:00
EndTime=00:00:00
UseDataDictionary=N
FileStorePath=store/test_noise_trader_server_store
FileLogPath=logs/test_noise_trader_server_logs
BeginString=FIX.4.2
)";

        for (int i = 1; i <= 5; ++i) {
            server_config_stream << "\n[SESSION]\n";
            server_config_stream << "SenderCompID=Edux-test" << i << "\n";
            server_config_stream << "TargetCompID=client" << i << "\n";
        }

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

TEST_F(NoiseTraderTest, MultipleTradersCanSubmitOrders) {
    const int num_traders = 5;
    std::vector<std::shared_ptr<NoiseTrader>> traders;
    std::vector<std::thread> trader_threads;

    for (int i = 1; i <= num_traders; ++i) {
        // Generate distinct configs for each client to prevent FileStore and CompID collisions
        std::stringstream client_config_stream;
        client_config_stream << R"(
[DEFAULT]
ConnectionType=initiator
ReconnectInterval=60
SocketConnectHost=localhost
SocketConnectPort=10005
UseDataDictionary=N
StartTime=00:00:00
EndTime=00:00:00
HeartBtInt=30
CheckLatency=N
BeginString=FIX.4.2
)";
        client_config_stream << "FileStorePath=store/test_noise_trader_client_store_" << i << "\n";
        client_config_stream << "FileLogPath=logs/test_noise_trader_client_log_" << i << "\n";
        client_config_stream << "\n[SESSION]\n";
        client_config_stream << "SenderCompID=client" << i << "\n";
        client_config_stream << "TargetCompID=Edux-test" << i << "\n";

        std::string config_path = "configs/temp/temp_client_" + std::to_string(i) + ".cfg";

        fs::path p = config_path;
        if (p.has_parent_path()) {
            fs::create_directories(p.parent_path()); // Creates all necessary directories
        }

        std::ofstream config_file(config_path);
        config_file << client_config_stream.str();
        config_file.close();

        // Instantiate generators
        auto process_gen = std::make_unique<PoissonProcessGenerator>(20.0); // Fast interval
        auto decision_gen = std::make_unique<BernoulliDecisionGenerator<OrderSide>>(0.5);
        auto quantity_gen = std::make_unique<ParetoQuantityGenerator<double>>(1.0, 2.0);

        // Instantiate FixClient
        auto fix_client = std::make_unique<TestClient>(config_path);
        fix_client->connect(10005);

        // Wait up to 5 seconds for connection
        int retries = 0;
        while (!fix_client->is_connected() && retries < 50) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            retries++;
        }

        EXPECT_TRUE(fix_client->is_connected()) << "Client " << i << " failed to connect to server.";

        // Instantiate Trader
        const std::vector<std::string> tickers = {"AAPL"};
        auto trader = std::make_shared<NoiseTrader>(
            std::move(process_gen),
            std::move(decision_gen),
            std::move(quantity_gen),
            std::move(fix_client),
            std::move(tickers)
        );

        traders.push_back(trader);

        // Spawn thread for strategy execution
        trader_threads.emplace_back([trader]() {
            trader->run_strategy();
        });
    }

    // Allow them to run and submit orders for a short duration
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Detach threads as run_strategy contains an infinite loop
    for (auto& t : trader_threads) {
        if (t.joinable()) {
            t.detach();
        }
    }

    // Success if we reached here without segmentation faults or QuickFIX aborts
    SUCCEED();
}
