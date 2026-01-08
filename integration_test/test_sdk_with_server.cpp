#include "../client_sdk/include/order.h"
#include "../client_sdk/include/test_server.h"
#include <filesystem>
#include <iostream>
#include <quickfix/FileStore.h>
#include <quickfix/SessionSettings.h>
#include <quickfix/SocketAcceptor.h>
namespace fs = std::filesystem;

#include "test_client.h"

int main() {
    fs::path serverConfigFileName = "example_config_server.cfg";
    fs::path pathToServerConfig =
        fs::current_path() / fs::path("client_sdk") / serverConfigFileName;
    std::cout << "Path to config: " << pathToServerConfig << '\n';
    const FIX::SessionSettings settings(pathToServerConfig.string());
    TestFixServer server;
    FIX::FileStoreFactory file_store_factory(settings);
    FIX::SocketAcceptor acceptor(server, file_store_factory, settings);
    acceptor.start();
    std::cout << "Server started" << '\n';

    fs::path clientConfigFileName = "example_config_client.cfg";
    fs::path pathToClientConfig =
        fs::current_path() / fs::path("client_sdk") / clientConfigFileName;
    std::cout << "Path to config: " << pathToClientConfig << '\n';
    const auto client = new TestClient(pathToClientConfig.string());
    client->connect(5);
    bool res = client->submit_limit_order("test_ticker", 1.0, 10.0, OrderSide::BUY,
                                          TimeInForce::GTC, "joshorder");
    bool res1 = client->cancel_order("test_ticker", OrderSide::BUY, "joshorder");
    std::cout << "Client started" << '\n';
    std::cout << "Enter any key to exit" << '\n';
    std::cin.ignore();
    acceptor.stop();
    client->disconnect();
    delete client;
}
