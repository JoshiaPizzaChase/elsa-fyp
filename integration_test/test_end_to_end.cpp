#include "test_client.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <sys/wait.h>
namespace fs = std::filesystem;

const fs::path gateway_bin_path = PROJECT_ROOT_DIR / fs::path("build-test") / fs::path("services") /
                                  fs::path("gateway") / fs::path("gateway");

TestClient set_up_test_client() {
    fs::path config_filename = "example_config_client.cfg";
    fs::path path_to_config =
        PROJECT_ROOT_DIR / fs::path("configs") / fs::path("test_client") / config_filename;
    return TestClient{path_to_config.string()};
}

// Starts gateway as a separate process, returning the child pid.
// Throws on failure.
pid_t set_up_gateway() {
    fs::path config_filename = "example_config_server.cfg";
    fs::path path_to_config = PROJECT_ROOT_DIR / fs::path("configs") / fs::path("gateway") /
                              fs::path("hk1") / config_filename;

    assert(fs::exists(gateway_bin_path) && fs::is_regular_file(gateway_bin_path) &&
           "Error in accessing Gateway binaries");

    pid_t pid = fork();
    if (pid < 0) {
        throw std::runtime_error("fork() failed");
    }

    if (pid == 0) {
        // Child: replace process image with gateway binary
        // argv: gateway --config <path>
        execl(gateway_bin_path.c_str(), path_to_config.c_str(), (char*)NULL);
        // If execl returns, it's an error
        // _exit(EXIT_FAILURE);
    }

    // Parent: give gateway a moment to start
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return pid;
}

// Stop gateway process
// Tries SIGTERM, waits, then SIGKILL if needed.
void stop_gateway(pid_t pid, std::chrono::seconds timeout = std::chrono::seconds(5)) {
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
    pid_t gateway_pid = -1;
    try {
        gateway_pid = set_up_gateway();
    } catch (const std::exception& e) {
        FAIL(std::string("Failed to start gateway: ") + e.what());
    }

    // auto test_client_1 = set_up_test_client();
    fs::path config_filename = "example_config_client.cfg";
    fs::path path_to_config =
        PROJECT_ROOT_DIR / fs::path("configs") / fs::path("test_client") / config_filename;
    auto test_client_1{set_up_test_client()};
    test_client_1.connect(5);

    // cleanup
    if (gateway_pid > 0) {
        stop_gateway(gateway_pid);
    }

    test_client_1.disconnect();
}
