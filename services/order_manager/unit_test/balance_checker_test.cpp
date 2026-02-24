#include <catch2/catch_test_macros.hpp>

#include "../src/balance_checker.h"

using namespace om;

constexpr std::string USD_TICKER{"USD"};

TEST_CASE("Updating balance of a broker", "[om]") {
    BalanceChecker balance_checker = BalanceChecker();

    SECTION("Updating balance of a new broker") {
        balance_checker.update_balance("67", USD_TICKER, 100);

        REQUIRE(balance_checker.get_balance("67", USD_TICKER) == 100);
    }

    SECTION("Updating balance of an existing broker") {
        balance_checker.update_balance("67", USD_TICKER, 100);
        balance_checker.update_balance("67", USD_TICKER, -100);

        REQUIRE(balance_checker.get_balance("67", USD_TICKER) == 0);
    }
}

TEST_CASE("Checking if broker has sufficient balance to be deducted", "[om]") {
    BalanceChecker balance_checker = BalanceChecker();

    balance_checker.update_balance("67", USD_TICKER, 100);

    REQUIRE(balance_checker.has_sufficient_balance("67", USD_TICKER, -50));
    REQUIRE(balance_checker.has_sufficient_balance("67", USD_TICKER, -200) == false);
}
