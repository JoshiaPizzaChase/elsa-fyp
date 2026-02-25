#include <catch2/catch_test_macros.hpp>

#include "../src/order_manager.h"

using namespace om;

constexpr std::string TEST_BROKER_ID = "broker_67";
constexpr std::string TEST_SYMBOL = "GME";

TEST_CASE("Processing incoming messages", "[om]") {
    BalanceChecker balance_checker{};

    auto new_order = core::NewOrderSingleContainer{.sender_comp_id = TEST_BROKER_ID,
                                                   .symbol = TEST_SYMBOL,

                                                   .time_in_force = core::TimeInForce::gtc};

    SECTION("Incoming buy orders") {
        balance_checker.update_balance(TEST_BROKER_ID, USD_SYMBOL, 1000);
        new_order.side = core::Side::bid;

        SECTION("Limit buy") {
            new_order.ord_type = core::OrderType::limit;
            new_order.order_qty = 10;

            SECTION("Sufficient capital") {
                new_order.price = 100;

                REQUIRE(om::validate_container(new_order, balance_checker));
            }

            SECTION("Insufficient capital") {
                new_order.price = 999;

                REQUIRE(om::validate_container(new_order, balance_checker) == false);
            }
        }

        SECTION("Incoming market buy order") {
            new_order.ord_type = core::OrderType::market;
            new_order.price = std::nullopt;

            // A mock ME with GME 100@$10

            SECTION("Sufficient capital") {
                new_order.order_qty = 10;

                REQUIRE(validate_container(new_order, balance_checker));
            }

            SECTION("Insufficient capital") {
                new_order.order_qty = 999;

                REQUIRE(validate_container(new_order, balance_checker) == false);
            }
        }
    }

    SECTION("Incoming sell orders") {
        balance_checker.update_balance(TEST_BROKER_ID, TEST_SYMBOL, 100);
        new_order.side = core::Side::ask;

        SECTION("Limit sell") {
            new_order.ord_type = core::OrderType::limit;
            new_order.price = 100;

            SECTION("Sufficient inventory") {
                new_order.order_qty = 100;

                REQUIRE(om::validate_container(new_order, balance_checker));
            }

            SECTION("Insufficient inventory") {
                new_order.order_qty = 999;

                REQUIRE(om::validate_container(new_order, balance_checker) == false);
            }
        }

        SECTION("Market sell") {
            new_order.ord_type = core::OrderType::market;
            new_order.price = std::nullopt;

            SECTION("Sufficient inventory") {
                new_order.order_qty = 100;

                REQUIRE(om::validate_container(new_order, balance_checker));
            }

            SECTION("Insufficient inventory") {
                new_order.order_qty = 999;

                REQUIRE(om::validate_container(new_order, balance_checker) == false);
            }
        }
    }
}
