#ifndef ELSA_FYP_ORDER_MANAGER_H
#define ELSA_FYP_ORDER_MANAGER_H

#include "balance_checker.h"

class OrderManager {
  private:
    BalanceChecker balance_checker = BalanceChecker();
};

#endif // ELSA_FYP_ORDER_MANAGER_H
