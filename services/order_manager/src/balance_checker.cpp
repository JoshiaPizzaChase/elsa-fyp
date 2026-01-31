#include "balance_checker.h"

#include <cassert>
#include <format>

namespace om {
void BalanceChecker::update_balance(const std::string& broker_id, int delta) {
    auto it = balance_map.find(broker_id);

    if (it != balance_map.end()) {
        assert(has_sufficient_balance(broker_id, delta) &&
               "Cannot deduct more fund than available");

        it->second += delta;
    } else {
        assert(delta >= 0 && "Cannot create new account balance with negative delta");

        balance_map.emplace(broker_id, delta);
    }
}

int BalanceChecker::get_balance(const std::string& broker_id) {
    assert(balance_map.contains(broker_id) && "Balance record of Broker ID does not exist");
    return balance_map[broker_id];
}

bool BalanceChecker::has_sufficient_balance(const std::string& broker_id, int delta) {
    assert(balance_map.contains(broker_id) && "Balance record of Broker ID does not exist");

    return get_balance(broker_id) + delta >= 0;
}
}
