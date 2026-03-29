CREATE TABLE IF NOT EXISTS orders_test
(
    ts
    TIMESTAMP,
    order_id
    INT,
    cl_order_id
    INT,
    sender_comp_id
    VARCHAR,
    symbol
    SYMBOL,
    side
    SYMBOL,
    order_qty
    INT,
    filled_qty
    INT,
    ord_type
    SYMBOL,
    price
    INT,
    time_in_force
    SYMBOL,
    order_status
    SYMBOL
) TIMESTAMP
(
    ts
) PARTITION BY DAY
    DEDUP UPSERT KEYS
(
    ts,
    order_id
);

CREATE TABLE IF NOT EXISTS trades_test
(
    ts
    TIMESTAMP,
    price
    INT,
    quantity
    INT,
    symbol
    SYMBOL,
    trade_id
    VARCHAR,
    taker_id
    VARCHAR,
    maker_id
    VARCHAR,
    taker_order_id
    INT,
    maker_order_id
    INT,
    is_taker_buyer
    BOOLEAN
) TIMESTAMP
(
    ts
) PARTITION BY DAY
    DEDUP UPSERT KEYS
(
    ts,
    trade_id
);
