#include "test_client.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <sys/wait.h>
namespace fs = std::filesystem;

const fs::path gateway_bin_path =
    PROJECT_BUILD_DIR / fs::path("services") / fs::path("gateway") / fs::path("gateway");

const fs::path order_manager_bin_path = PROJECT_BUILD_DIR / fs::path("services") /
                                        fs::path("order_manager") / fs::path("order_manager");

const fs::path matching_engine_bin_path = PROJECT_BUILD_DIR / fs::path("services") /
                                          fs::path("matching_engine") / fs::path("matching_engine");

const fs::path market_data_processor_bin_path = PROJECT_BUILD_DIR / fs::path("services") /
                                                fs::path("market_data_processor") /
                                                fs::path("market_data_processor");

TestClient set_up_test_client() {
    fs::path config_filename = "example_config_client.cfg";
    fs::path path_to_config =
        PROJECT_ROOT_DIR / fs::path("configs") / fs::path("test_client") / config_filename;
    return TestClient{path_to_config.string()};
}

// Starts gateway as a separate process, returning the child pid.
// Throws on failure.
pid_t set_up_gateway() {
    fs::path fix_config_filename{"example_config_server.cfg"};
    fs::path path_to_fix_config{PROJECT_ROOT_DIR / fs::path("configs") / fs::path("gateway") /
                                fs::path("hk01") / fix_config_filename};
    assert(fs::exists(gateway_bin_path) && fs::is_regular_file(gateway_bin_path) &&
           "Error in accessing Gateway binaries");

    fs::path gateway_config_filename{"gateway01.toml"};
    fs::path path_to_gateway_config{PROJECT_ROOT_DIR / fs::path("configs") / fs::path("gateway") /
                                    fs::path("hk01") / gateway_config_filename};

    pid_t pid = fork();
    if (pid < 0) {
        throw std::runtime_error("fork() failed");
    }

    if (pid == 0) {
        // Child: replace process image with gateway binary
        // argv: <gateway binary path> <FIX config path> <gateway config path>
        execl(gateway_bin_path.c_str(), gateway_bin_path.c_str(), path_to_fix_config.c_str(),
              path_to_gateway_config.c_str(), (char*)NULL);
        // If execl returns, it's an error
        _exit(EXIT_FAILURE);
    }

    // Parent: give gateway a moment to start
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return pid;
}

pid_t set_up_order_manager() {
    fs::path order_manager_config_filename{"order_manager01.toml"};
    fs::path path_to_order_manger_config{PROJECT_ROOT_DIR / fs::path("configs") /
                                         fs::path("order_manager") / fs::path("hk01") /
                                         order_manager_config_filename};
    pid_t pid = fork();
    if (pid < 0) {
        throw std::runtime_error("fork() failed");
    }

    if (pid == 0) {
        // Child: replace process image with order manager binary
        // argv: <order manager bin path> <order manager config path>
        execl(order_manager_bin_path.c_str(), order_manager_bin_path.c_str(),
              path_to_order_manger_config.c_str(), (char*)NULL);
        // If execl returns, it's an error
        _exit(EXIT_FAILURE);
    }

    // Parent: give order manager a moment to start
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return pid;
}

pid_t set_up_matching_engine() {
    fs::path matching_engine_config_filename{"matching_engine01.toml"};
    fs::path path_to_matching_engine_config{PROJECT_ROOT_DIR / fs::path("configs") /
                                            fs::path("matching_engine") / fs::path("hk01") /
                                            matching_engine_config_filename};
    pid_t pid = fork();
    if (pid < 0) {
        throw std::runtime_error("fork() failed");
    }

    if (pid == 0) {
        // Child: replace process image with matching engine binary
        // argv: <matching engine binary path> <matching engine config path>
        execl(matching_engine_bin_path.c_str(), matching_engine_bin_path.c_str(),
              path_to_matching_engine_config.c_str(), (char*)NULL);
        // If execl returns, it's an error
        _exit(EXIT_FAILURE);
    }

    // Parent: give matching engine a moment to start
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return pid;
}

pid_t set_up_market_data_processor() {
    fs::path market_data_processor_config_filename{"mdp01.toml"};
    fs::path path_to_market_data_processor_config{PROJECT_ROOT_DIR / fs::path("configs") /
                                                  fs::path("mdp") / fs::path("hk01") /
                                                  market_data_processor_config_filename};
    pid_t pid = fork();
    if (pid < 0) {
        throw std::runtime_error("fork() failed");
    }

    if (pid == 0) {
        // Child: replace process image with market data processor binary
        // argv: <market data processor binary path> <market data processor config path>
        execl(market_data_processor_bin_path.c_str(), market_data_processor_bin_path.c_str(),
              path_to_market_data_processor_config.c_str(), (char*)NULL);
        // If execl returns, it's an error
        _exit(EXIT_FAILURE);
    }

    // Parent: give market data processor a moment to start
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return pid;
}

// Tries SIGTERM, waits, then SIGKILL if needed.
void stop_process(pid_t pid, std::chrono::seconds timeout = std::chrono::seconds(5)) {
    if (pid <= 0)
        return;

    // Send SIGTERM
    kill(pid, SIGTERM);

    // wait with timeout
    auto start = std::chrono::steady_clock::now();
    while (true) {
        int status = 0;
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid)
            return; // exited
        if (r == -1) {
            // already reaped or error
            return;
        }
        if (std::chrono::steady_clock::now() - start > timeout)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Force kill
    kill(pid, SIGKILL);
    waitpid(pid, nullptr, 0);
}

TEST_CASE("Submit order requests", "[integration]") {
    // Setup Market Data Processor
    pid_t market_data_processor_pid = -1;
    try {
        market_data_processor_pid = set_up_market_data_processor();
    } catch (const std::exception& e) {
        FAIL(std::string("Failed to start market data processor: ") + e.what());
    }

    // Setup Matching Engine
    pid_t matching_engine_pid = -1;
    try {
        matching_engine_pid = set_up_matching_engine();
    } catch (const std::exception& e) {
        FAIL(std::string("Failed to start matching engine: ") + e.what());
    }

    // Setup Order Manager
    pid_t order_manager_pid = -1;
    try {
        order_manager_pid = set_up_order_manager();
    } catch (const std::exception& e) {
        FAIL(std::string("Failed to start order manager: ") + e.what());
    }

    // Setup Gateway
    pid_t gateway_pid = -1;
    try {
        gateway_pid = set_up_gateway();
    } catch (const std::exception& e) {
        FAIL(std::string("Failed to start gateway: ") + e.what());
    }

    // Setup client
    auto test_client_1 = set_up_test_client();

    test_client_1.connect(5);

    // test_client_1.submit_limit_order("GME", 100.0, 10.0, OrderSide::BUY, TimeInForce::GTC,
    // "1234"); test_client_1.submit_limit_order("GME", 100.0, 10.0, OrderSide::BUY,
    // TimeInForce::GTC, "1234"); test_client_1.submit_limit_order("GME", 100.0, 10.0,
    // OrderSide::BUY, TimeInForce::GTC, "1234"); test_client_1.submit_limit_order("GME",
    // 100.0, 10.0, OrderSide::BUY, TimeInForce::GTC, "1234");

    // sleep(10);
    while (true) {
        test_client_1.submit_limit_order("GME", 100.0, 10.0, OrderSide::BUY, TimeInForce::GTC,
                                         "1234");
        sleep(1);
    }

    // cleanup
    if (gateway_pid > 0) {
        stop_process(gateway_pid);
    }

    if (order_manager_pid > 0) {
        stop_process(order_manager_pid);
    }

    if (matching_engine_pid > 0) {
        stop_process(matching_engine_pid);
    }

    if (market_data_processor_pid > 0) {
        stop_process(market_data_processor_pid);
    }
}
