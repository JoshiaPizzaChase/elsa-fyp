-- Set up the tables
-- 1. users
CREATE TABLE IF NOT EXISTS users 
(
    user_id INT PRIMARY KEY,
    username VARCHAR UNIQUE NOT NULL,
    password VARCHAR NOT NULL,
    created_ts TIMESTAMPTZ NOT NULL,
    last_modified_ts TIMESTAMPTZ NOT NULL
);
-- 2. servers
CREATE TABLE IF NOT EXISTS servers 
(
    server_id INT PRIMARY KEY,
    server_name VARCHAR UNIQUE NOT NULL,
    admin_id INT REFERENCES users(user_id),
    created_ts TIMESTAMPTZ,
    last_modified_ts TIMESTAMPTZ
);
-- 3. whitelist
CREATE TABLE IF NOT EXISTS allowlist
(
    server_id INT REFERENCES servers(server_id),
    user_id INT REFERENCES users(user_id),
    created_ts TIMESTAMPTZ NOT NULL,
    PRIMARY KEY(server_id, user_id)
);
-- 4. balances
CREATE TABLE IF NOT EXISTS balances
(
    user_id INT REFERENCES users(user_id),
    symbol VARCHAR(4) NOT NULL,
    balance INT NOT NULL,
    created_ts TIMESTAMPTZ NOT NULL,
    modified_ts TIMESTAMPTZ NOT NULL,
    PRIMARY KEY(user_id, symbol)
);

