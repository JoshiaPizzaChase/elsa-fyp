#include "configuration/gateway_config.h"
#include "gateway_application.h"
#include "quickfix/FileStore.h"
#include "quickfix/SessionSettings.h"
#include "quickfix/SocketAcceptor.h"
#include "rfl/toml/load.hpp"
#include <filesystem>
#include <print>

namespace fs = std::filesystem;

/* Entry point for FIX gateway services */
int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::println("argc: {}", argc);
        std::println("Please pass in program name as arg0");
        std::println("Please pass in FIX server config path as arg1");
        std::println("Please pass in gateway config as arg2");
        return -1;
    }

    // Read FIX config
    std::cout << "Reading FIX config file at: " << argv[1] << std::endl;
    auto path_to_config = fs::path(argv[1]);
    FIX::SessionSettings settings(path_to_config);

    // Read Gateway config
    std::cout << "Reading Gateway config file at: " << argv[2] << std::endl;
    gateway::GatewayConfig gateway_config =
        rfl::toml::load<gateway::GatewayConfig>(argv[2]).value();

    gateway::GatewayApplication application{gateway_config.downstream_order_manager_host,
                                            gateway_config.downstream_order_manager_port};

    FIX::FileStoreFactory storeFactory(settings);
    FIX::ScreenLogFactory logFactory(settings);
    FIX::SocketAcceptor acceptor(application, storeFactory, settings, logFactory);

    acceptor.start();
    while (true) {
    }
    acceptor.stop();
}
