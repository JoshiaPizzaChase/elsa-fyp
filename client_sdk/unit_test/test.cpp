#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <iostream>
namespace fs = std::filesystem;

#include "test_client.h"

int test_fix() {
    try {
        fs::path configFileName = "example_config_client.cfg";
        fs::path pathToConfig = fs::current_path() / fs::path("client_sdk") / configFileName;
        auto client = new TestClient(pathToConfig.string());
        client->connect(5);
        return 0;
    } catch (std::exception& e) {
        std::cout << e.what() << std::endl;
        return -1;
    }
}

TEST_CASE("QuickFIX unit_test", "[QuickFIX]") {
    REQUIRE(test_fix() == 0);
}
