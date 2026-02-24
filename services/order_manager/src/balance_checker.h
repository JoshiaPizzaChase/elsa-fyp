#pragma once

#include <string>
#include <unordered_map>

namespace om {
class BalanceChecker {
  public:
    void update_balance(const std::string& broker_id, const std::string& ticker, int delta);
    int get_balance(const std::string& broker_id, const std::string& ticker);
    bool has_sufficient_balance(const std::string& broker_id, const std::string& ticker, int delta);

  private:
    std::unordered_map<std::string, std::unordered_map<std::string, int>>
        balance_map; // Map(broker_id, Map(ticker, amount))
};
} // namespace om
