#include "balance_checker.h"

#include <boost/contract.hpp>
#include <cassert>

namespace om {
bool BalanceChecker::broker_id_exists(const std::string& broker_id) const {
    return balance_map.contains(broker_id);
}

bool BalanceChecker::broker_owns_ticker(const std::string& broker_id,
                                        const std::string& ticker) const {
    boost::contract::check c = boost::contract::public_function(this).precondition([&] {
        BOOST_CONTRACT_ASSERT(broker_id_exists(broker_id));
        BOOST_CONTRACT_ASSERT(!ticker.empty());
    });

    return balance_map.at(broker_id).contains(ticker);
}

void BalanceChecker::update_balance(const std::string& broker_id, const std::string& ticker,
                                    int delta) {
    boost::contract::check c = boost::contract::public_function(this).precondition(
        [&] { BOOST_CONTRACT_ASSERT(!ticker.empty()); });

    // Check if broker has record
    if (const auto broker_it = balance_map.find(broker_id); broker_it != balance_map.end()) {
        // Check if broker has ticker's record
        if (const auto ticker_it = broker_it->second.find(ticker);
            ticker_it != broker_it->second.end()) {
            // Check if broker has sufficient balance
            assert(has_sufficient_balance(broker_id, ticker, delta) &&
                   "Cannot deduct more balance than available");

            ticker_it->second += delta;
        } else {
            broker_it->second.emplace(ticker, delta);
        }
    } else {
        balance_map.emplace(broker_id, std::unordered_map<std::string, int>{{ticker, delta}});
    }
}

int BalanceChecker::get_balance(const std::string& broker_id, const std::string& ticker) const {
    int rtn;
    boost::contract::check c = boost::contract::public_function(this)
                                   .precondition([&] {
                                       BOOST_CONTRACT_ASSERT(broker_id_exists(broker_id));
                                       BOOST_CONTRACT_ASSERT(broker_owns_ticker(broker_id, ticker));
                                   })
                                   .postcondition([&] { BOOST_CONTRACT_ASSERT(rtn >= 0); });

    return rtn = balance_map.at(broker_id).at(ticker);
}

bool BalanceChecker::has_sufficient_balance(const std::string& broker_id, const std::string& ticker,
                                            int delta) const {
    boost::contract::check c = boost::contract::public_function(this).precondition([&] {
        BOOST_CONTRACT_ASSERT(broker_id_exists(broker_id));
        BOOST_CONTRACT_ASSERT(broker_owns_ticker(broker_id, ticker));
    });

    return get_balance(broker_id, ticker) + delta >= 0;
}
} // namespace om