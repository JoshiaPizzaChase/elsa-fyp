#include "balance_checker.h"

#include <boost/contract.hpp>
#include <cassert>
#include <format>

namespace om {
bool BalanceChecker::broker_id_exists(const std::string& broker_id) const {
    return balance_map.contains(broker_id);
}

bool BalanceChecker::broker_owns_ticker(const std::string& broker_id,
                                        const std::string& ticker) const {
    boost::contract::check c = boost::contract::public_function(this).precondition(
        [&] { BOOST_CONTRACT_ASSERT(broker_id_exists(broker_id)); });

    return balance_map.at(broker_id).contains(ticker);
}

void BalanceChecker::update_balance(const std::string& broker_id, const std::string& ticker,
                                    int delta) {
    auto broker_it = balance_map.find(broker_id);

    // Check if broker has record
    if (broker_it != balance_map.end()) {
        // Check if broker has ticker's record
        auto ticker_it = broker_it->second.find(ticker);

        if (ticker_it != broker_it->second.end()) {
            // Check if broker has sufficient balance
            assert(has_sufficient_balance(broker_id, ticker, delta) &&
                   "Cannot deduct more balance than available");

            ticker_it->second += delta;
        } else {
            assert(delta >= 0 && "Cannot create new account balance with negative delta");

            broker_it->second.emplace(ticker, delta);
        }
    } else {
        assert(delta >= 0 && "Cannot create new account balance with negative delta");

        balance_map.emplace(broker_id, std::unordered_map<std::string, int>{{ticker, delta}});
    }
}

int BalanceChecker::get_balance(const std::string& broker_id, const std::string& ticker) const {
    assert(broker_id_exists(broker_id) && "Balance record of Broker ID does not exist");
    assert(broker_owns_ticker(broker_id, ticker) && "Broker does not have record of ticker");
    return balance_map.at(broker_id).at(ticker);
}

bool BalanceChecker::has_sufficient_balance(const std::string& broker_id, const std::string& ticker,
                                            int delta) const {
    assert(broker_id_exists(broker_id) && "Balance record of Broker ID does not exist");
    assert(broker_owns_ticker(broker_id, ticker) && "Broker does not have record of ticker");
    return get_balance(broker_id, ticker) + delta >= 0;
}
} // namespace om