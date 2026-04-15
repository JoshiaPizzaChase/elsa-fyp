#include <benchmark/benchmark.h>

#include "websocket_client.h"
#include "websocket_server.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>

namespace {
using namespace std::chrono_literals;

bool wait_for_open(transport::WebsocketManager<transport::Client>& manager, int id,
                   std::chrono::milliseconds timeout = 5s) {
    const auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < timeout) {
        const auto metadata = manager.get_metadata(id);
        if (metadata && metadata->get_status() == transport::ConnectionStatus::open) {
            return true;
        }
        std::this_thread::sleep_for(5ms);
    }
    return false;
}

std::optional<int> wait_for_first_open_server_id(transport::WebsocketManagerServer& manager,
                                                 std::chrono::milliseconds timeout = 5s) {
    const auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < timeout) {
        for (const auto& [id, metadata] : manager.get_id_to_connection_map()) {
            if (metadata && metadata->get_status() == transport::ConnectionStatus::open) {
                return id;
            }
        }
        std::this_thread::sleep_for(5ms);
    }
    return std::nullopt;
}

std::shared_ptr<spdlog::logger> get_or_create_stdout_logger(std::string_view logger_name) {
    if (const auto existing = spdlog::get(std::string{logger_name}); existing) {
        existing->set_level(spdlog::level::err);
        return existing;
    }
    auto logger = spdlog::stdout_color_mt(std::string{logger_name});
    logger->set_level(spdlog::level::err);
    return logger;
}

void BM_Websocket_ClientToServerRoundTripLatency(benchmark::State& state) {
    constexpr int kPort = 39091;
    const std::string uri = "localhost";
    const std::string ws_uri = std::format("ws://localhost:{}", kPort);
    const std::string payload(static_cast<std::size_t>(state.range(0)), 'x');
    const auto server_logger = get_or_create_stdout_logger("ws_benchmark_server");
    const auto client_logger = get_or_create_stdout_logger("ws_benchmark_client");

    transport::WebsocketManagerServer server{kPort, uri, server_logger};
    if (!server.start().has_value()) {
        state.SkipWithError("Failed to start websocket benchmark server.");
        return;
    }

    transport::WebsocketManagerClient client{client_logger};
    if (!client.start().has_value()) {
        state.SkipWithError("Failed to start websocket benchmark client.");
        return;
    }

    const auto client_connection = client.connect(ws_uri, "ws_benchmark_client");
    if (!client_connection.has_value()) {
        state.SkipWithError("Failed to connect websocket benchmark client.");
        return;
    }

    const int client_id = *client_connection;
    if (!wait_for_open(client, client_id)) {
        state.SkipWithError("Websocket benchmark client connection did not open in time.");
        return;
    }

    const auto server_id = wait_for_first_open_server_id(server);
    if (!server_id.has_value()) {
        state.SkipWithError("Websocket benchmark server connection did not open in time.");
        return;
    }

    for (auto _ : state) {
        const auto start = std::chrono::steady_clock::now();

        if (!client.send(client_id, payload, transport::MessageFormat::text).has_value()) {
            state.SkipWithError("Failed to send benchmark message.");
            break;
        }

        while (true) {
            auto message = server.dequeue_message(*server_id);
            if (message.has_value()) {
                benchmark::DoNotOptimize(*message);
                break;
            }
            std::this_thread::yield();
        }

        const auto end = std::chrono::steady_clock::now();
        const auto latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        state.SetIterationTime(static_cast<double>(latency_ns.count()) / 1e9);
    }

    state.SetBytesProcessed(state.iterations() * payload.size());
}

} // namespace

BENCHMARK(BM_Websocket_ClientToServerRoundTripLatency)
    ->Arg(64)
    ->Arg(512)
    ->Arg(4096)
    ->UseManualTime()
    ->Unit(benchmark::kNanosecond);

BENCHMARK_MAIN();
