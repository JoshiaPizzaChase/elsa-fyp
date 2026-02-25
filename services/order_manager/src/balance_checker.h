#ifndef ELSA_FYP_BALANCE_CHECKER_H
#define ELSA_FYP_BALANCE_CHECKER_H

#include <string>
#include <unordered_map>

namespace om {
class BalanceChecker {
  public:
    void update_balance(const std::string& broker_id, const std::string& ticker, int delta);
    int get_balance(const std::string& broker_id, const std::string& ticker) const;
    bool has_sufficient_balance(const std::string& broker_id, const std::string& ticker,
                                int delta) const;

  private:
    std::unordered_map<std::string, std::unordered_map<std::string, int>>
        balance_map; // Map(broker_id, Map(ticker, amount))
};
} // namespace om

#endif // ELSA_FYP_BALANCE_CHECKER_H
