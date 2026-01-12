#include "../src/gateway_application.h"
#include "quickfix/FileStore.h"
#include "quickfix/SessionSettings.h"
#include "quickfix/SocketAcceptor.h"
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    try {
        fs::path configFileName = "example_config_server.cfg";
        fs::path pathToConfig = fs::current_path() / configFileName;

        FIX::SessionSettings settings(pathToConfig);

        gateway::GatewayApplication application;
        FIX::FileStoreFactory storeFactory(settings);
        FIX::ScreenLogFactory logFactory(settings);
        FIX::SocketAcceptor acceptor(application, storeFactory, settings, logFactory);

        acceptor.start();
        acceptor.stop();
        return 0;
    } catch (std::exception& e) {
        std::cout << e.what() << '\n';
        return 1;
    }
}
