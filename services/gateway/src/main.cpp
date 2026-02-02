#include "gateway_application.h"
#include "quickfix/FileStore.h"
#include "quickfix/SessionSettings.h"
#include "quickfix/SocketAcceptor.h"
#include <filesystem>
#include <print>

namespace fs = std::filesystem;

/* Entry point for FIX gateway services */
int main(int argc, char* argv[]) {
    if (argc != 1) {
        std::println("argc: {}", argc);
        std::println("Please pass in FIX server config path as argument");
    }
    std::cout << "Reading config file at: " << argv[0] << std::endl;

    auto path_to_config = fs::path(argv[0]);
    FIX::SessionSettings settings(path_to_config);

    gateway::GatewayApplication application;
    FIX::FileStoreFactory storeFactory(settings);
    FIX::ScreenLogFactory logFactory(settings);
    FIX::SocketAcceptor acceptor(application, storeFactory, settings, logFactory);

    acceptor.start();
    while (true) {
    }
    acceptor.stop();
}
