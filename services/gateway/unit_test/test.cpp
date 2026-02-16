#include "../src/gateway_application.h"
#include "quickfix/FileStore.h"
#include "quickfix/SessionSettings.h"
#include "quickfix/SocketAcceptor.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <iostream>
#include <websocketpp/roles/client_endpoint.hpp>

#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>

using client = websocketpp::client<websocketpp::config::asio_client>;

using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

// pull out the type of messages sent by our config
using message_ptr = websocketpp::config::asio_client::message_type::ptr;

namespace fs = std::filesystem;

int test_gateway_start_stop() {
    try {
        fs::path configFileName = "example_config_server.cfg";
        fs::path pathToConfig{PROJECT_SOURCE_DIR / fs::path("configs") / fs::path("gateway") /
                              fs::path("hk01") / configFileName};

        FIX::SessionSettings settings(pathToConfig);

        gateway::GatewayApplication application{"aaaa", 123};
        FIX::FileStoreFactory storeFactory(settings);
        FIX::ScreenLogFactory logFactory(settings);
        FIX::SocketAcceptor acceptor(application, storeFactory, settings, logFactory);

        acceptor.start();
        // while (true) {
        // }
        acceptor.stop();

        return 0;
    } catch (std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

TEST_CASE("Gateway Application starts and stops properly") {
    REQUIRE(test_gateway_start_stop() == 0);
}
