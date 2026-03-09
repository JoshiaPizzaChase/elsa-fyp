#include "order.h"

#include <gtest/gtest.h>

using namespace engine;

class OrderTest : public testing::Test {
  protected:
    OrderTest() : order{0, 100, 10, Side::bid} {
    }

    Order order;
};
using OrderDeathTest = OrderTest;

TEST_F(OrderDeathTest, FillInvalidQuantity) {
    EXPECT_DEATH(order.fill(100), "");
}

TEST_F(OrderTest, FillValidQuantity) {
    order.fill(1);

    EXPECT_EQ(order.get_quantity(), 9);
}