#ifndef ELSA_FYP_BALANCE_CHECKER_H
#define ELSA_FYP_BALANCE_CHECKER_H

#include <string>
#include <unordered_map>

namespace om {
class BalanceChecker {
public:
  void update_balance(const std::string& broker_id, int delta);
  int get_balance(const std::string& broker_id);
  bool has_sufficient_balance(const std::string& broker_id, int delta);

private:
  std::unordered_map<std::string, int> balance_map;
};
}

#endif // ELSA_FYP_BALANCE_CHECKER_H
