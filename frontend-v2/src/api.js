const API_BASE = process.env.REACT_APP_API_BASE ?? '';

async function fetchJSON(path, params = {}) {
    const url = new URL(path, window.location.origin);
    url.search = new URLSearchParams(params).toString();
    if (API_BASE) {
        url.href = `${API_BASE}${url.pathname}${url.search}`;
    }
    const res = await fetch(url.toString());
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return res.json();
}

async function postJSON(path, body, username) {
    let url = path;
    if (API_BASE) url = `${API_BASE}${path}`;
    const res = await fetch(url, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            ...(username ? {'Authorization': `Bearer ${username}`} : {}),
        },
        body: JSON.stringify(body),
    });
    const data = await res.json();
    if (!res.ok) throw Object.assign(new Error(`HTTP ${res.status}`), {data});
    return data;
}

export async function login(user_name, password) {
    return fetchJSON('/login', {user_name, password});
}

export async function signup(user_name, password) {
    return fetchJSON('/signup', {user_name, password});
}

export async function getActiveServers() {
    return fetchJSON('/active_servers');
}

export async function getActiveSymbols(server_name) {
    return fetchJSON('/active_symbols', {server_name});
}

export async function getUserServers(user_name) {
    return fetchJSON('/user_servers', {user_name});
}

export async function getAccountDetails(user_name, server_name) {
    return fetchJSON('/get_account_details', {user_name, server_name});
}

export async function getHistoricalTrades(server, symbol, after_ts_ms) {
    return fetchJSON('/get_historical_trades', {server, symbol, after_ts_ms});
}

/**
 * Create a new trading server.
 * @param {string} username - the logged-in admin's username (used for auth token)
 * @param {{ server_name, description, mdp_endpoint, active_symbols: string[], allowlist: string[] }} payload
 */
export async function createServer(username, payload) {
    return postJSON('/create_server', payload, username);
}

/**
 * Update settings of an existing server.
 * Only the server's admin may call this successfully.
 * @param {string} username - the logged-in admin's username (used for auth token)
 * @param {{ server_name, description, mdp_endpoint, active_symbols: string[], allowlist: string[] }} payload
 */
export async function configureServer(username, payload) {
    return postJSON('/configure_server', payload, username);
}

