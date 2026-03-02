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

} // namespace backend::queries

