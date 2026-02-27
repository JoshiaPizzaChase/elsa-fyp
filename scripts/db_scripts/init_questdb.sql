CREATE TABLE IF NOT EXISTS orders
(
    ts TIMESTAMP,
    order_id INT,
    cl_order_id INT,
    sender_comp_id VARCHAR,
    symbol SYMBOL,
    side SYMBOL,
    order_qty INT,
    filled_qty INT,
    ord_type INT,
    price INT,
    time_in_force SYMBOL
) TIMESTAMP(ts) PARTITION BY DAY 
DEDUP UPSERT KEYS(ts, order_id);
    
CREATE TABLE IF NOT EXISTS trades 
(
    ts TIMESTAMP,
    price INT,
    quantity INT,
    symbol SYMBOL,
    trade_id INT,
    taker_id INT,
    maker_id INT,
    taker_order_id INT,
    maker_order_id INT,
    is_taker_buy BOOLEAN
) TIMESTAMP(ts) PARTITION BY DAY 
DEDUP UPSERT KEYS(ts, trade_id);
