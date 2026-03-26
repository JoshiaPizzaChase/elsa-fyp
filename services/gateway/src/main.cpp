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
    // Read FIX config
    auto fix_cfg = argc < 2 ? "gateway_server.cfg" : fs::path(argv[1]);
    FIX::SessionSettings settings(fix_cfg);

    // Read Gateway config
    auto gateway_cfg = argc < 3 ? "gateway.toml" : argv[2];
    gateway::GatewayConfig gateway_config =
        rfl::toml::load<gateway::GatewayConfig>(gateway_cfg).value();

    gateway::GatewayApplication application{gateway_config.downstream_order_manager_host,
                                            gateway_config.downstream_order_manager_port};

    FIX::FileStoreFactory storeFactory(settings);
    FIX::ScreenLogFactory logFactory(settings);
    FIX::SocketAcceptor acceptor(application, storeFactory, settings, logFactory);

    acceptor.start();
    while (true) {
        application.process_report();
    }
    acceptor.stop();
}
