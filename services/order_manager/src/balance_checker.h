#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace om {
class BalanceChecker {
  public:
    bool broker_id_exists(const std::string& broker_id) const;
    bool broker_owns_ticker(const std::string& broker_id, const std::string& ticker) const;

    void update_balance(const std::string& broker_id, const std::string& ticker, std::int64_t delta);
    std::int64_t get_balance(const std::string& broker_id, const std::string& ticker) const;
    bool has_sufficient_balance(const std::string& broker_id, const std::string& ticker,
                                std::int64_t delta) const;

  private:
    std::unordered_map<std::string, std::unordered_map<std::string, std::int64_t>>
        balance_map; // Map(broker_id, Map(ticker, amount))
};
} // namespace om
