#include "../src/limit_order_book.h"
#include "../src/order.h"
#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace engine;

TEST_CASE("Adding order to limit order book", "[lob]") {
    LimitOrderBook limit_order_book;

    SECTION("Adding invalid orders") {
        // Order ID collision
        limit_order_book.add_order(67, 100, 10, Side::Bid);
        REQUIRE(limit_order_book.add_order(67, 100, 10, Side::Bid).has_value() == false);

        // Non-positive price
        REQUIRE(limit_order_book.add_order(68, 0, 10, Side::Ask).has_value() == false);
        REQUIRE(limit_order_book.add_order(69, -100, 10, Side::Ask).has_value() == false);

        // Non-positive quantity
        REQUIRE(limit_order_book.add_order(70, 100, 0, Side::Bid).has_value() == false);
        REQUIRE(limit_order_book.add_order(71, 100, -10, Side::Bid).has_value() == false);
    }

    SECTION("Adding order without matching") {
        limit_order_book.add_order(67, 100, 10, Side::Bid);
        REQUIRE(limit_order_book.get_best_order(Side::Bid).has_value());
        const Order& best_bid = limit_order_book.get_best_order(Side::Bid).value();
        REQUIRE(best_bid.get_order_id() == 67);
        REQUIRE(best_bid.get_price() == 100);
        REQUIRE(best_bid.get_quantity() == 10);
        REQUIRE(best_bid.get_side() == Side::Bid);

        limit_order_book.add_order(68, 101, 10, Side::Ask);
        REQUIRE(limit_order_book.get_best_order(Side::Ask).has_value());
        const Order& best_ask = limit_order_book.get_best_order(Side::Ask).value();
        REQUIRE(best_ask.get_order_id() == 68);
        REQUIRE(best_ask.get_price() == 101);
        REQUIRE(best_ask.get_quantity() == 10);
        REQUIRE(best_ask.get_side() == Side::Ask);
    }

    SECTION("Matching existing bid order completely") {
        limit_order_book.add_order(67, 100, 10, Side::Bid);
        limit_order_book.add_order(68, 100, 10, Side::Ask);

        REQUIRE(limit_order_book.get_best_order(Side::Bid).has_value() == false);
        REQUIRE(limit_order_book.get_order_by_id(67).has_value() == false);

        REQUIRE(limit_order_book.get_best_order(Side::Ask).has_value() == false);
        REQUIRE(limit_order_book.get_order_by_id(68).has_value() == false);
    }

    SECTION("Matching existing ask order completely") {
        limit_order_book.add_order(67, 100, 10, Side::Ask);
        limit_order_book.add_order(68, 100, 10, Side::Bid);

        REQUIRE(limit_order_book.get_best_order(Side::Bid).has_value() == false);
        REQUIRE(limit_order_book.get_order_by_id(67).has_value() == false);

        REQUIRE(limit_order_book.get_best_order(Side::Ask).has_value() == false);
        REQUIRE(limit_order_book.get_order_by_id(68).has_value() == false);
    }

    SECTION("Partially filling a standing bid order and completely filling a new ask order") {
        limit_order_book.add_order(67, 100, 10, Side::Bid);
        limit_order_book.add_order(68, 100, 5, Side::Ask);

        REQUIRE(limit_order_book.get_best_order(Side::Bid).has_value() == true);
        const Order& best_bid = limit_order_book.get_best_order(Side::Bid).value();
        REQUIRE(best_bid.get_order_id() == 67);
        REQUIRE(best_bid.get_price() == 100);
        REQUIRE(best_bid.get_quantity() == 5);
        REQUIRE(best_bid.get_side() == Side::Bid);

        REQUIRE(limit_order_book.get_best_order(Side::Ask).has_value() == false);
        REQUIRE(limit_order_book.get_order_by_id(68).has_value() == false);
    }

    SECTION("Partially filling a standing ask order and completely filling a new bid order") {
        limit_order_book.add_order(67, 100, 10, Side::Ask);
        limit_order_book.add_order(68, 100, 5, Side::Bid);

        REQUIRE(limit_order_book.get_best_order(Side::Ask).has_value() == true);
        const Order& best_ask = limit_order_book.get_best_order(Side::Ask).value();
        REQUIRE(best_ask.get_order_id() == 67);
        REQUIRE(best_ask.get_price() == 100);
        REQUIRE(best_ask.get_quantity() == 5);
        REQUIRE(best_ask.get_side() == Side::Ask);

        REQUIRE(limit_order_book.get_best_order(Side::Bid).has_value() == false);
        REQUIRE(limit_order_book.get_order_by_id(68).has_value() == false);
    }

    SECTION("Completely filling a standing bid order and partially filling a new ask order") {
        limit_order_book.add_order(67, 100, 5, Side::Bid);
        limit_order_book.add_order(68, 100, 10, Side::Ask);

        REQUIRE(limit_order_book.get_best_order(Side::Bid).has_value() == false);
        REQUIRE(limit_order_book.get_order_by_id(67).has_value() == false);

        REQUIRE(limit_order_book.get_best_order(Side::Ask).has_value() == true);
        const Order& best_ask = limit_order_book.get_best_order(Side::Ask).value();
        REQUIRE(best_ask.get_order_id() == 68);
        REQUIRE(best_ask.get_price() == 100);
        REQUIRE(best_ask.get_quantity() == 5);
        REQUIRE(best_ask.get_side() == Side::Ask);
    }

    SECTION("Completely filling a standing ask order and partially filling a new bid order") {
        limit_order_book.add_order(67, 100, 5, Side::Ask);
        limit_order_book.add_order(68, 100, 10, Side::Bid);

        REQUIRE(limit_order_book.get_best_order(Side::Ask).has_value() == false);
        REQUIRE(limit_order_book.get_order_by_id(67).has_value() == false);

        REQUIRE(limit_order_book.get_best_order(Side::Bid).has_value() == true);
        const Order& best_bid = limit_order_book.get_best_order(Side::Bid).value();
        REQUIRE(best_bid.get_order_id() == 68);
        REQUIRE(best_bid.get_price() == 100);
        REQUIRE(best_bid.get_quantity() == 5);
        REQUIRE(best_bid.get_side() == Side::Bid);
    }

    SECTION("Matching multiple standing bid orders") {
        limit_order_book.add_order(67, 100, 5, Side::Bid);
        limit_order_book.add_order(68, 99, 5, Side::Bid);
        limit_order_book.add_order(69, 98, 5, Side::Bid);
        limit_order_book.add_order(70, 98, 20, Side::Ask);

        REQUIRE(limit_order_book.get_best_order(Side::Bid).has_value() == false);
        REQUIRE(limit_order_book.get_order_by_id(67).has_value() == false);
        REQUIRE(limit_order_book.get_order_by_id(68).has_value() == false);
        REQUIRE(limit_order_book.get_order_by_id(69).has_value() == false);

        REQUIRE(limit_order_book.get_best_order(Side::Ask).has_value());
        const Order& best_ask = limit_order_book.get_best_order(Side::Ask).value();
        REQUIRE(best_ask.get_order_id() == 70);
        REQUIRE(best_ask.get_price() == 98);
        REQUIRE(best_ask.get_quantity() == 5);
        REQUIRE(best_ask.get_side() == Side::Ask);
    }

    SECTION("Matching multiple standing ask orders") {
        limit_order_book.add_order(67, 100, 5, Side::Ask);
        limit_order_book.add_order(68, 101, 5, Side::Ask);
        limit_order_book.add_order(69, 102, 5, Side::Ask);
        limit_order_book.add_order(70, 102, 20, Side::Bid);

        REQUIRE(limit_order_book.get_best_order(Side::Ask).has_value() == false);
        REQUIRE(limit_order_book.get_order_by_id(67).has_value() == false);
        REQUIRE(limit_order_book.get_order_by_id(68).has_value() == false);
        REQUIRE(limit_order_book.get_order_by_id(69).has_value() == false);

        REQUIRE(limit_order_book.get_best_order(Side::Bid).has_value());
        const Order& best_bid = limit_order_book.get_best_order(Side::Bid).value();
        REQUIRE(best_bid.get_order_id() == 70);
        REQUIRE(best_bid.get_price() == 102);
        REQUIRE(best_bid.get_quantity() == 5);
        REQUIRE(best_bid.get_side() == Side::Bid);
    }

    SECTION("Adding a partially filled market bid order") {
        limit_order_book.add_order(67, 100, 10, Side::Ask);
        limit_order_book.add_order(68, 101, 10, Side::Ask);
        limit_order_book.add_order(69, 99, 10, Side::Bid);
        limit_order_book.add_order(70, std::numeric_limits<int>::max(), 25,
                                   Side::Bid); // Market order

        REQUIRE(limit_order_book.get_best_order(Side::Ask).has_value() == false);
        REQUIRE(limit_order_book.get_best_order(Side::Bid).has_value());
        const Order& best_bid = limit_order_book.get_best_order(Side::Bid).value();
        REQUIRE(best_bid.get_order_id() == 70);
        REQUIRE(best_bid.get_price() == std::numeric_limits<int>::max());
        REQUIRE(best_bid.get_quantity() == 5);
        REQUIRE(best_bid.get_side() == Side::Bid);
    }

    SECTION("Adding a partially filled market ask order") {
        limit_order_book.add_order(67, 100, 10, Side::Bid);
        limit_order_book.add_order(68, 99, 10, Side::Bid);
        limit_order_book.add_order(69, 101, 10, Side::Ask);
        limit_order_book.add_order(70, std::numeric_limits<int>::min(), 25,
                                   Side::Ask); // Market order

        REQUIRE(limit_order_book.get_best_order(Side::Bid).has_value() == false);
        REQUIRE(limit_order_book.get_best_order(Side::Ask).has_value());
        const Order& best_ask = limit_order_book.get_best_order(Side::Ask).value();
        REQUIRE(best_ask.get_order_id() == 70);
        REQUIRE(best_ask.get_price() == std::numeric_limits<int>::min());
        REQUIRE(best_ask.get_quantity() == 5);
        REQUIRE(best_ask.get_side() == Side::Ask);
    }
}

TEST_CASE("Getting best order from empty limit order book", "[lob]") {
    LimitOrderBook limit_order_book;

    REQUIRE(limit_order_book.get_best_order(Side::Bid).has_value() == false);
    REQUIRE(limit_order_book.get_best_order(Side::Ask).has_value() == false);
}

TEST_CASE("Getting order by ID from limit order book", "[lob]") {
    LimitOrderBook limit_order_book;

    SECTION("Getting existing order") {
        limit_order_book.add_order(67, 100, 10, Side::Bid);
        REQUIRE(limit_order_book.get_order_by_id(67).has_value());
        const Order& order = limit_order_book.get_order_by_id(67).value();
        REQUIRE(order.get_order_id() == 67);
        REQUIRE(order.get_price() == 100);
        REQUIRE(order.get_quantity() == 10);
        REQUIRE(order.get_side() == Side::Bid);
    }

    SECTION("Getting non-existing order") {
        REQUIRE(limit_order_book.get_order_by_id(68).has_value() == false);
    }
}

TEST_CASE("Cancelling order in limit order book", "[lob]") {
    LimitOrderBook limit_order_book;

    SECTION("Cancelling non-existing order") {
        REQUIRE(limit_order_book.cancel_order(67).has_value() == false);
    }

    SECTION("Cancelling existing order") {
        limit_order_book.add_order(67, 100, 10, Side::Bid);
        REQUIRE(limit_order_book.cancel_order(67).has_value());
        REQUIRE(limit_order_book.get_order_by_id(67).has_value() == false);
    }
}

TEST_CASE("Getting level aggregate", "[lob]") {
    LimitOrderBook limit_order_book;

    SECTION("Getting level aggregate of empty order book") {
        auto level_aggregate = limit_order_book.get_level_aggregate(Side::Bid, 0);
        REQUIRE(level_aggregate.has_value() == false);
    }

    SECTION("Getting level aggregate with valid level number") {
        limit_order_book.add_order(67, 100, 10, Side::Bid);
        limit_order_book.add_order(68, 100, 10, Side::Bid);
        limit_order_book.add_order(69, 101, 10, Side::Bid);

        auto level_zero_aggregate = limit_order_book.get_level_aggregate(Side::Bid, 0).value();
        auto level_one_aggregate = limit_order_book.get_level_aggregate(Side::Bid, 1).value();

        REQUIRE(level_zero_aggregate.price == 101);
        REQUIRE(level_zero_aggregate.quantity == 10);

        REQUIRE(level_one_aggregate.price == 100);
        REQUIRE(level_one_aggregate.quantity == 20);
    }

    SECTION("Getting level aggregate with invalid level number") {
        limit_order_book.add_order(67, 100, 10, Side::Bid);

        auto level_ten_aggregate = limit_order_book.get_level_aggregate(Side::Bid, 10);

        REQUIRE(level_ten_aggregate.has_value() == false);
    }
}
