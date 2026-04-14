DO $$
BEGIN
    IF NOT EXISTS (SELECT 1 FROM pg_type WHERE typname = 'service_type_enum') THEN
CREATE TYPE service_type_enum AS ENUM ('gateway', 'oms', 'me', 'mdp', 'deployment_server');
ELSE
ALTER TYPE service_type_enum ADD VALUE IF NOT EXISTS 'deployment_server';
END IF;
END $$;

-- TODO: Function to update timestamp on modification

-- Set up the tables
-- 1. users
CREATE TABLE IF NOT EXISTS users
(
    user_id SERIAL PRIMARY KEY,
    username VARCHAR UNIQUE NOT NULL, -- also used as sender_comp_id
    password VARCHAR NOT NULL,
    created_ts TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_modified_ts TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);
-- 2. servers
CREATE TABLE IF NOT EXISTS servers
(
    server_id SERIAL PRIMARY KEY,
    server_name VARCHAR UNIQUE NOT NULL,
    admin_id INT REFERENCES users(user_id),
    created_ts TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_modified_ts TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    active_tickers VARCHAR(10)[],
    description VARCHAR(255),
    initial_usd INT NOT NULL DEFAULT 100000
);

ALTER TABLE servers
    ADD COLUMN IF NOT EXISTS initial_usd INT NOT NULL DEFAULT 100000;
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
    server_id INT REFERENCES servers(server_id),
    symbol VARCHAR(10) NOT NULL,
    balance BIGINT NOT NULL,
    created_ts TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    modified_ts TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY(user_id, symbol, server_id)
);
-- 5. machines
CREATE TABLE IF NOT EXISTS machines
(
    machine_id SERIAL PRIMARY KEY,
    machine_name VARCHAR(10) UNIQUE NOT NULL,
    ip INET NOT NULL,
    created_ts TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    modified_ts TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);
-- Insert local machine if it doesn't exist
INSERT INTO machines (machine_name, ip)
SELECT 'localhost', '127.0.0.1'
    WHERE NOT EXISTS (
    SELECT 1 FROM machines WHERE machine_name = 'localhost' OR ip = '127.0.0.1'
);

-- 6. services
CREATE TABLE IF NOT EXISTS services
(
    server_id INT REFERENCES servers(server_id) ON DELETE CASCADE,
    machine_id INT REFERENCES machines(machine_id) ON DELETE CASCADE,
    service_type service_type_enum NOT NULL,
    port INT NOT NULL,
    created_ts TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    modified_ts TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY(server_id, machine_id, port),
    CHECK (port BETWEEN 1 AND 65535)
);
