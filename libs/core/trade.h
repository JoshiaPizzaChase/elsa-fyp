#ifndef ELSA_FYP_TRADE_H
#define ELSA_FYP_TRADE_H

#include "core/orderbook_snapshot.h"

struct Trade {
    int trade_id{0};
    int taker_order_id{0};
    int maker_order_id{0};

    char ticker[MAX_TICKER_LENGTH]{};
    int price{0};
    int quantity{0};
};

#endif // ELSA_FYP_TRADE_H
