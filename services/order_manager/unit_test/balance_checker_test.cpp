#include "balance_checker.h"
#include <gtest/gtest.h>

using namespace om;

constexpr std::string_view BROKER_ID_1{"BROKER_1"};
constexpr std::string_view BROKER_ID_2{"BROKER_2"};
constexpr std::string_view TICKER_1{"AAPL"};
constexpr std::string_view TICKER_2{"GOOGL"};

// =====================================================================
// BrokerIDExistsTest
// =====================================================================

class BrokerIDExistsTest : public testing::Test {
  protected:
    BalanceChecker balance_checker;
};

TEST_F(BrokerIDExistsTest, BrokerIDDoesNotExist) {
    EXPECT_FALSE(balance_checker.broker_id_exists(std::string{BROKER_ID_1}));
}

TEST_F(BrokerIDExistsTest, BrokerIDExists) {
    balance_checker.update_balance(std::string{BROKER_ID_1}, std::string{TICKER_1}, 1000);
    EXPECT_TRUE(balance_checker.broker_id_exists(std::string{BROKER_ID_1}));
}

TEST_F(BrokerIDExistsTest, MultipleBrokerIDsExist) {
    balance_checker.update_balance(std::string{BROKER_ID_1}, std::string{TICKER_1}, 1000);
    balance_checker.update_balance(std::string{BROKER_ID_2}, std::string{TICKER_1}, 500);

    EXPECT_TRUE(balance_checker.broker_id_exists(std::string{BROKER_ID_1}));
    EXPECT_TRUE(balance_checker.broker_id_exists(std::string{BROKER_ID_2}));
}

// =====================================================================
// BrokerOwnTickerTest
// =====================================================================

class BrokerOwnTickerTest : public testing::Test {
  protected:
    BalanceChecker balance_checker;

    void SetUp() override {
        balance_checker.update_balance(std::string{BROKER_ID_1}, std::string{TICKER_1}, 1000);
    }
};
using BrokerOwnTickerDeathTest = BrokerOwnTickerTest;

TEST_F(BrokerOwnTickerDeathTest, BrokerDoesNotExist) {
    EXPECT_DEATH(std::ignore = balance_checker.broker_owns_ticker(std::string{BROKER_ID_2},
                                                                  std::string{TICKER_1}),
                 "");
}

TEST_F(BrokerOwnTickerDeathTest, EmptyTicker) {
    EXPECT_DEATH(std::ignore = balance_checker.broker_owns_ticker(std::string{BROKER_ID_1}, ""),
                 "");
}

TEST_F(BrokerOwnTickerTest, BrokerOwnsTicker) {
    EXPECT_TRUE(
        balance_checker.broker_owns_ticker(std::string{BROKER_ID_1}, std::string{TICKER_1}));
}

TEST_F(BrokerOwnTickerTest, BrokerDoesNotOwnTicker) {
    EXPECT_FALSE(
        balance_checker.broker_owns_ticker(std::string{BROKER_ID_1}, std::string{TICKER_2}));
}

TEST_F(BrokerOwnTickerTest, MultipleTickersOwned) {
    balance_checker.update_balance(std::string{BROKER_ID_1}, std::string{TICKER_2}, 500);

    EXPECT_TRUE(
        balance_checker.broker_owns_ticker(std::string{BROKER_ID_1}, std::string{TICKER_1}));
    EXPECT_TRUE(
        balance_checker.broker_owns_ticker(std::string{BROKER_ID_1}, std::string{TICKER_2}));
}

// =====================================================================
// UpdateBalanceTest
// =====================================================================

class UpdateBalanceTest : public testing::Test {
  protected:
    BalanceChecker balance_checker;
};
using UpdateBalanceDeathTest = UpdateBalanceTest;

TEST_F(UpdateBalanceDeathTest, EmptyTicker) {
    EXPECT_DEATH(balance_checker.update_balance(std::string{BROKER_ID_1}, "", 100), "");
}

TEST_F(UpdateBalanceTest, AddNewBrokerTicker) {
    balance_checker.update_balance(std::string{BROKER_ID_1}, std::string{TICKER_1}, 1000);

    EXPECT_TRUE(balance_checker.broker_id_exists(std::string{BROKER_ID_1}));
    EXPECT_TRUE(
        balance_checker.broker_owns_ticker(std::string{BROKER_ID_1}, std::string{TICKER_1}));
    EXPECT_EQ(balance_checker.get_balance(std::string{BROKER_ID_1}, std::string{TICKER_1}), 1000);
}

TEST_F(UpdateBalanceTest, AddNewTickerToExistingBroker) {
    balance_checker.update_balance(std::string{BROKER_ID_1}, std::string{TICKER_1}, 1000);
    balance_checker.update_balance(std::string{BROKER_ID_1}, std::string{TICKER_2}, 500);

    EXPECT_EQ(balance_checker.get_balance(std::string{BROKER_ID_1}, std::string{TICKER_1}), 1000);
    EXPECT_EQ(balance_checker.get_balance(std::string{BROKER_ID_1}, std::string{TICKER_2}), 500);
}

TEST_F(UpdateBalanceTest, IncrementExistingBalance) {
    balance_checker.update_balance(std::string{BROKER_ID_1}, std::string{TICKER_1}, 1000);
    balance_checker.update_balance(std::string{BROKER_ID_1}, std::string{TICKER_1}, 500);

    EXPECT_EQ(balance_checker.get_balance(std::string{BROKER_ID_1}, std::string{TICKER_1}), 1500);
}

TEST_F(UpdateBalanceTest, DecrementExistingBalance) {
    balance_checker.update_balance(std::string{BROKER_ID_1}, std::string{TICKER_1}, 1000);
    balance_checker.update_balance(std::string{BROKER_ID_1}, std::string{TICKER_1}, -300);

    EXPECT_EQ(balance_checker.get_balance(std::string{BROKER_ID_1}, std::string{TICKER_1}), 700);
}

TEST_F(UpdateBalanceTest, DecrementToZero) {
    balance_checker.update_balance(std::string{BROKER_ID_1}, std::string{TICKER_1}, 1000);
    balance_checker.update_balance(std::string{BROKER_ID_1}, std::string{TICKER_1}, -1000);

    EXPECT_EQ(balance_checker.get_balance(std::string{BROKER_ID_1}, std::string{TICKER_1}), 0);
}

TEST_F(UpdateBalanceDeathTest, DecrementBelowZero) {
    balance_checker.update_balance(std::string{BROKER_ID_1}, std::string{TICKER_1}, 1000);
    EXPECT_DEATH(
        balance_checker.update_balance(std::string{BROKER_ID_1}, std::string{TICKER_1}, -1001), "");
}

TEST_F(UpdateBalanceTest, MultipleBalancesForDifferentBrokers) {
    balance_checker.update_balance(std::string{BROKER_ID_1}, std::string{TICKER_1}, 1000);
    balance_checker.update_balance(std::string{BROKER_ID_2}, std::string{TICKER_1}, 500);

    EXPECT_EQ(balance_checker.get_balance(std::string{BROKER_ID_1}, std::string{TICKER_1}), 1000);
    EXPECT_EQ(balance_checker.get_balance(std::string{BROKER_ID_2}, std::string{TICKER_1}), 500);
}

// =====================================================================
// GetBalanceTest
// =====================================================================

class GetBalanceTest : public testing::Test {
  protected:
    BalanceChecker balance_checker;

    void SetUp() override {
        balance_checker.update_balance(std::string{BROKER_ID_1}, std::string{TICKER_1}, 1000);
        balance_checker.update_balance(std::string{BROKER_ID_1}, std::string{TICKER_2}, 500);
    }
};
using GetBalanceDeathTest = GetBalanceTest;

TEST_F(GetBalanceDeathTest, BrokerIDDoesNotExist) {
    EXPECT_DEATH(std::ignore =
                     balance_checker.get_balance(std::string{BROKER_ID_2}, std::string{TICKER_1}),
                 "");
}

TEST_F(GetBalanceDeathTest, BrokerDoesNotOwnTicker) {
    EXPECT_DEATH(std::ignore = balance_checker.get_balance(std::string{BROKER_ID_1}, "TSLA"), "");
}

TEST_F(GetBalanceTest, RetrieveValidBalance) {
    EXPECT_EQ(balance_checker.get_balance(std::string{BROKER_ID_1}, std::string{TICKER_1}), 1000);
}

TEST_F(GetBalanceTest, RetrieveMultipleBalances) {
    EXPECT_EQ(balance_checker.get_balance(std::string{BROKER_ID_1}, std::string{TICKER_1}), 1000);
    EXPECT_EQ(balance_checker.get_balance(std::string{BROKER_ID_1}, std::string{TICKER_2}), 500);
}

TEST_F(GetBalanceTest, RetrieveZeroBalance) {
    balance_checker.update_balance(std::string{BROKER_ID_1}, std::string{TICKER_1}, -1000);
    EXPECT_EQ(balance_checker.get_balance(std::string{BROKER_ID_1}, std::string{TICKER_1}), 0);
}

// =====================================================================
// HasSufficientBalanceTest
// =====================================================================

class HasSufficientBalanceTest : public testing::Test {
  protected:
    BalanceChecker balance_checker;

    void SetUp() override {
        balance_checker.update_balance(std::string{BROKER_ID_1}, std::string{TICKER_1}, 1000);
    }
};
using HasSufficientBalanceDeathTest = HasSufficientBalanceTest;

TEST_F(HasSufficientBalanceDeathTest, BrokerIDDoesNotExist) {
    EXPECT_DEATH(std::ignore = balance_checker.has_sufficient_balance(std::string{BROKER_ID_2},
                                                                      std::string{TICKER_1}, 100),
                 "");
}

TEST_F(HasSufficientBalanceDeathTest, BrokerDoesNotOwnTicker) {
    EXPECT_DEATH(std::ignore = balance_checker.has_sufficient_balance(std::string{BROKER_ID_1},
                                                                      std::string{TICKER_2}, 100),
                 "");
}

TEST_F(HasSufficientBalanceTest, HasSufficientBalance) {
    EXPECT_TRUE(balance_checker.has_sufficient_balance(std::string{BROKER_ID_1},
                                                       std::string{TICKER_1}, -500));
}

TEST_F(HasSufficientBalanceTest, HasExactBalance) {
    EXPECT_TRUE(balance_checker.has_sufficient_balance(std::string{BROKER_ID_1},
                                                       std::string{TICKER_1}, -1000));
}

TEST_F(HasSufficientBalanceTest, InsufficientBalance) {
    EXPECT_FALSE(balance_checker.has_sufficient_balance(std::string{BROKER_ID_1},
                                                        std::string{TICKER_1}, -1001));
}

TEST_F(HasSufficientBalanceTest, ZeroDelta) {
    EXPECT_TRUE(
        balance_checker.has_sufficient_balance(std::string{BROKER_ID_1}, std::string{TICKER_1}, 0));
}
