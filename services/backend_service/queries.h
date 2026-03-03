#pragma once

#include <string_view>

namespace backend::queries {

// /login?user_name=...&password=...
// Params: $1 = username, $2 = password
// Returns: 1 row if credentials match, 0 rows otherwise
constexpr std::string_view LOGIN = R"(
    SELECT user_id, username
    FROM users
    WHERE username = $1 AND password = $2
)";

// /signup?user_name=...&password=...
// Params: $1 = username, $2 = password
// Returns: the newly created user_id, or error if username already exists
constexpr std::string_view SIGNUP = R"(
    INSERT INTO users (username, password)
    VALUES ($1, $2)
    RETURNING user_id, username
)";

// /active_servers
// No params
// Returns: all servers with admin username
constexpr std::string_view ACTIVE_SERVERS = R"(
    SELECT s.server_id, s.server_name, u.username AS admin_name,
           s.active_symbols, s.created_ts, s.last_modified_ts
    FROM servers s
    JOIN users u ON s.admin_id = u.user_id
)";

// /user_info?user_name=...
// Params: $1 = username
// Returns: user row
constexpr std::string_view USER_INFO = R"(
    SELECT user_id, username, created_ts, last_modified_ts
    FROM users
    WHERE username = $1
)";

// /user_info (balances part)
// Params: $1 = user_id (obtained from USER_INFO query)
// Returns: all balances for the user
constexpr std::string_view USER_BALANCES = R"(
    SELECT symbol, balance, created_ts, modified_ts
    FROM balances
    WHERE user_id = $1
)";

// /active_symbols?server_name=...
// Params: $1 = server_name
// Returns: active_symbols array for the given server
constexpr std::string_view ACTIVE_SYMBOLS = R"(
    SELECT server_name, active_symbols
    FROM servers
    WHERE server_name = $1
)";

// /user_servers?user_name=...
// Params: $1 = username
// Returns: all servers the user is associated with, as admin or member
constexpr std::string_view USER_SERVERS = R"(
    SELECT s.server_id, s.server_name, s.active_symbols,
           s.created_ts, s.last_modified_ts,
           CASE WHEN s.admin_id = u.user_id THEN 'admin' ELSE 'member' END AS role
    FROM users u
    JOIN (
        SELECT server_id FROM servers WHERE admin_id = (SELECT user_id FROM users WHERE username = $1)
        UNION
        SELECT a.server_id FROM allowlist a JOIN users u2 ON a.user_id = u2.user_id WHERE u2.username = $1
    ) AS user_servers ON TRUE
    JOIN servers s ON s.server_id = user_servers.server_id
    WHERE u.username = $1
)";

// /user_servers (per-server balances)
// Params: $1 = user_id, $2 = server active_symbols (used by application logic)
// Returns: balances for the user filtered to the server's active symbols
constexpr std::string_view USER_SERVER_BALANCES = R"(
    SELECT symbol, balance
    FROM balances
    WHERE user_id = $1
)";

// /get_historical_trades?server=...&symbol=...
// Params: $1 = symbol
// Returns: all trades for the symbol in the past 2 hours, oldest first
constexpr std::string_view GET_HISTORICAL_TRADES = R"(
    SELECT trade_id, symbol, price, quantity, ts
    FROM trades
    WHERE symbol = $1
      AND ts >= dateadd('h', -2, now())
    ORDER BY ts ASC
)";

} // namespace backend::queries

