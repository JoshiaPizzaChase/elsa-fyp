#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <iostream>
namespace fs = std::filesystem;

#include "test_client.h"

int test_fix() {
    try {
        fs::path configFileName = "example_config_client.cfg";
        const fs::path pathToConfig =
            PROJECT_SOURCE_DIR / fs::path("configs") / fs::path("test_client") / configFileName;

        auto* client = new TestClient(pathToConfig.string());
        client->connect(5);
        std::cout << client->submit_limit_order("GME", 10.0, 100, OrderSide::BUY, TimeInForce::GTC, 8964) << std::endl;
        return 0;
    } catch (std::exception& e) {
        std::cout << e.what() << '\n';
        return -1;
    }
}

TEST_CASE("QuickFIX unit_test", "[QuickFIX]") {
    REQUIRE(test_fix() == 0);
}
