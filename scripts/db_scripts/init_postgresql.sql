-- TODO: Function to update timestamp on modification

-- Set up the tables
-- 1. users
CREATE TABLE IF NOT EXISTS users 
(
    user_id INT PRIMARY KEY,
    username VARCHAR UNIQUE NOT NULL,
    password VARCHAR NOT NULL,
    created_ts TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_modified_ts TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);
-- 2. servers
CREATE TABLE IF NOT EXISTS servers 
(
    server_id INT PRIMARY KEY,
    server_name VARCHAR UNIQUE NOT NULL,
    admin_id INT REFERENCES users(user_id),
    created_ts TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_modified_ts TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    active_tickers VARCHAR(10)[]
);
-- 3. whitelist
CREATE TABLE IF NOT EXISTS allowlist
(
    server_id INT REFERENCES servers(server_id),
    user_id INT REFERENCES users(user_id),
    created_ts TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY(server_id, user_id)
);
-- 4. balances
CREATE TABLE IF NOT EXISTS balances
(
    user_id INT REFERENCES users(user_id),
    symbol VARCHAR(10) NOT NULL,
    balance INT NOT NULL,
    created_ts TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    modified_ts TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY(user_id, symbol)
);

