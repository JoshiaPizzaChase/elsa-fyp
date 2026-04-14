#pragma once

#include "spdlog/async.h"
#include "spdlog/cfg/env.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"
#include <csignal>
#include <cstdlib>
#include <string>

inline void shutdown_handler(int signal_number) {
    spdlog::apply_all([](const std::shared_ptr<spdlog::logger>& active_logger) {
        active_logger->info("program shutting down");
        active_logger->flush();
    });
    spdlog::shutdown();
    std::_Exit(128 + signal_number);
}

inline auto init_spdlog_res = [] {
    spdlog::cfg::load_env_levels();
    spdlog::flush_every(std::chrono::seconds{5});
    std::atexit([] { spdlog::shutdown(); });
    std::signal(SIGINT, shutdown_handler);
    std::signal(SIGTERM, shutdown_handler);
    std::signal(SIGABRT, shutdown_handler);
#ifdef SIGQUIT
    std::signal(SIGQUIT, shutdown_handler);
#endif
#ifdef SIGHUP
    std::signal(SIGHUP, shutdown_handler);
#endif
    return 0;
}();

namespace logger {
static std::shared_ptr<spdlog::logger> create_logger(const std::string_view& logger_name,
                                                     const std::string_view& log_output_path) {
    return spdlog::basic_logger_mt<spdlog::async_factory>(logger_name.data(),
                                                          log_output_path.data());
}
} // namespace logger
