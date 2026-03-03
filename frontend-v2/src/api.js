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

export async function login(user_name, password) {
    return fetchJSON('/login', {user_name, password});
}

export async function signup(user_name, password) {
    return fetchJSON('/signup', {user_name, password});
}

export async function getActiveServers() {
    return fetchJSON('/active_servers');
}

export async function getUserInfo(user_name) {
    return fetchJSON('/user_info', {user_name});
}

export async function getActiveSymbols(server_name) {
    return fetchJSON('/active_symbols', {server_name});
}

export async function getUserServers(user_name) {
    return fetchJSON('/user_servers', {user_name});
}

export async function getHistoricalTrades(server, symbol) {
    return fetchJSON('/get_historical_trades', {server, symbol});
}

