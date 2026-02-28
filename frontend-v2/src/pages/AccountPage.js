import React, {useState} from 'react';
import {useAuth} from '../context/AuthContext';
import {PieChart, Pie, Cell, Tooltip, ResponsiveContainer, Legend} from 'recharts';
import './AccountPage.css';

// ---------------------------------------------------------------------------
// TODO: Replace with real DB queries.
//       fetchUserServers(username) → [{
//         id, name, description, users, mdpEndpoint, role ('admin'|'member'),
//         totalTrades, balances: [{ ticker, quantity }]
//       }]
//       fetchLatestPrices(tickers) → { [ticker]: price }
// ---------------------------------------------------------------------------

// TODO: fetch latest price per ticker from DB / market data feed
const DUMMY_PRICES = {
    AAPL: 0.7823,
    GOOGL: 0.6541,
    MSFT: 0.8910,
    AMZN: 0.7105,
    TSLA: 0.5630,
    META: 0.9244,
    NVDA: 0.8872,
    JPM: 0.7390,
};

const DUMMY_SERVERS = [
    {
        id: 101,
        name: 'FINA4999',
        description: 'My personal quant lab for algo strategy testing.',
        users: 5,
        mdpEndpoint: 'ws://localhost:9001',
        role: 'admin',
        totalTrades: 312,
        balances: [
            {ticker: 'AAPL', quantity: 15980},
            {ticker: 'GOOGL', quantity: 12750},
            {ticker: 'MSFT', quantity: 17060},
            {ticker: 'NVDA', quantity: 23670},
        ],
    },
    {
        id: 102,
        name: 'FINA3999',
        description: 'Options pricing simulation room.',
        users: 3,
        mdpEndpoint: 'ws://localhost:9001',
        role: 'admin',
        totalTrades: 87,
        balances: [
            {ticker: 'AMZN', quantity: 5770},
            {ticker: 'TSLA', quantity: 12040},
            {ticker: 'META', quantity: 3460},
            {ticker: 'JPM', quantity: 12790},
        ],
    },
    {
        id: 1,
        name: 'FINA2303',
        description: 'Financial Markets & Institutions — Prof. Wong.',
        users: 24,
        mdpEndpoint: 'ws://localhost:9001',
        role: 'member',
        totalTrades: 45,
        balances: [
            {ticker: 'AAPL', quantity: 4090},
            {ticker: 'JPM', quantity: 2440},
        ],
    },
    {
        id: 3,
        name: 'FINA3203',
        description: 'Derivative Securities — Prof. Li.',
        users: 37,
        mdpEndpoint: 'ws://localhost:9001',
        role: 'member',
        totalTrades: 128,
        balances: [
            {ticker: 'TSLA', quantity: 9950},
            {ticker: 'NVDA', quantity: 4730},
            {ticker: 'META', quantity: 3140},
        ],
    },
];

// TODO: fetch per-server initial USD balance from DB
const INITIAL_BALANCE = 100000;

const PIE_COLORS = [
    '#a29bfe', '#6c5ce7', '#74b9ff', '#0984e3',
    '#55efc4', '#00b894', '#ffeaa7', '#fdcb6e',
];

function ServerModal({server, onClose}) {
    const isNew = server === null;
    return (
        <div className="modal-overlay" onClick={onClose}>
            <div className="modal-box" onClick={(e) => e.stopPropagation()}>
                <h2 className="modal-title">
                    {isNew ? 'Create Server' : `Edit — ${server.name}`}
                </h2>
                <p className="modal-placeholder">Server settings — Coming Soon</p>
                <button className="modal-close-btn" onClick={onClose}>Close</button>
            </div>
        </div>
    );
}

function AccountPage() {
    const {user} = useAuth();
    const [selectedId, setSelectedId] = useState(DUMMY_SERVERS[0]?.id ?? null);
    const [modalServer, setModalServer] = useState(undefined);

    const servers = DUMMY_SERVERS;
    const prices = DUMMY_PRICES;
    const adminServers = servers.filter((s) => s.role === 'admin');
    const memberServers = servers.filter((s) => s.role === 'member');
    const selected = servers.find((s) => s.id === selectedId) ?? null;
    const totalValue = selected
        ? selected.balances.reduce((sum, b) => sum + b.quantity * (prices[b.ticker] ?? 0), 0)
        : 0;
    const pnl = totalValue - INITIAL_BALANCE;
    const pnlPct = (pnl / INITIAL_BALANCE) * 100;

    return (
        <div className="account-page">

            {/* ── Left sidebar ──────────────────────────────────── */}
            <aside className="account-sidebar">
                <div className="account-avatar">
                    {user?.username?.[0]?.toUpperCase() ?? '?'}
                </div>
                <h2 className="account-username">{user?.username ?? '—'}</h2>
                <p className="account-role">Student Trader</p>

                <div className="account-divider"/>

                {/* Admin servers */}
                <div className="account-nav-section">
                    <div className="account-nav-section-header">
                        <span className="account-nav-section-label">Managing</span>
                        <button className="account-create-btn" onClick={() => setModalServer(null)}>
                            + Create
                        </button>
                    </div>
                    {adminServers.length === 0 ? (
                        <p className="account-nav-empty">No servers yet.</p>
                    ) : adminServers.map((srv) => (
                        <button
                            key={srv.id}
                            className={`account-nav-item${selectedId === srv.id ? ' active' : ''}`}
                            onClick={() => setSelectedId(srv.id)}
                        >
                            <span className="account-nav-name">{srv.name}</span>
                            <span className="account-nav-badge admin">Admin</span>
                        </button>
                    ))}
                </div>

                <div className="account-divider"/>

                {/* Member servers */}
                <div className="account-nav-section">
                    <div className="account-nav-section-header">
                        <span className="account-nav-section-label">Member Of</span>
                    </div>
                    {memberServers.length === 0 ? (
                        <p className="account-nav-empty">Not a member of any server.</p>
                    ) : memberServers.map((srv) => (
                        <button
                            key={srv.id}
                            className={`account-nav-item${selectedId === srv.id ? ' active' : ''}`}
                            onClick={() => setSelectedId(srv.id)}
                        >
                            <span className="account-nav-name">{srv.name}</span>
                            <span className="account-nav-badge member">Member</span>
                        </button>
                    ))}
                </div>
            </aside>

            {/* ── Right detail view ────────────────────────────── */}
            <main className="account-main">
                {selected === null ? (
                    <div className="account-empty-state">Select a server to view details.</div>
                ) : (
                    <>
                        {/* Server header */}
                        <div className="account-detail-header">
                            <div className="account-detail-title-row">
                                <h2 className="account-detail-name">{selected.name}</h2>
                                <span className={`account-detail-badge ${selected.role}`}>
                                    {selected.role === 'admin' ? 'Admin' : 'Member'}
                                </span>
                            </div>
                            <p className="account-detail-desc">{selected.description}</p>
                            <p className="account-detail-endpoint">{selected.mdpEndpoint}</p>
                            {selected.role === 'admin' && (
                                <button
                                    className="account-edit-btn"
                                    onClick={() => setModalServer(selected)}
                                >
                                    Edit Settings
                                </button>
                            )}
                        </div>

                        {/* Stats */}
                        <div className="account-stat-row">
                            <div className="account-stat">
                                <span className="account-stat-value">
                                    ${totalValue.toLocaleString('en-US', {minimumFractionDigits: 2})}
                                </span>
                                <span className="account-stat-label">Portfolio Value</span>
                            </div>
                            <div className="account-stat">
                                <span className={`account-stat-value ${pnl >= 0 ? 'pnl-profit' : 'pnl-loss'}`}>
                                    {pnl >= 0 ? '+' : ''}${pnl.toLocaleString('en-US', {minimumFractionDigits: 2})}
                                </span>
                                <span className={`account-stat-pct ${pnl >= 0 ? 'pnl-profit' : 'pnl-loss'}`}>
                                    {pnl >= 0 ? '▲' : '▼'} {Math.abs(pnlPct).toFixed(2)}%
                                </span>
                                <span className="account-stat-label">Total P&amp;L</span>
                            </div>
                            <div className="account-stat">
                                <span className="account-stat-value">{selected.users}</span>
                                <span className="account-stat-label">Users Online</span>
                            </div>
                            <div className="account-stat">
                                <span className="account-stat-value">{selected.balances.length}</span>
                                <span className="account-stat-label">Positions</span>
                            </div>
                            <div className="account-stat">
                                <span className="account-stat-value">{selected.totalTrades}</span>
                                <span className="account-stat-label">Total Trades</span>
                            </div>
                        </div>

                        {/* Balances */}
                        <section className="account-panel">
                            <div className="account-panel-header">
                                <h3 className="account-panel-title">Balances</h3>
                                <div className="account-panel-meta">
                                    <span>
                                        Portfolio Value&nbsp;
                                        <strong>${totalValue.toLocaleString('en-US', {minimumFractionDigits: 2})}</strong>
                                    </span>
                                    <span className={`account-panel-pnl ${pnl >= 0 ? 'pnl-profit' : 'pnl-loss'}`}>
                                        P&amp;L&nbsp;
                                        <strong>
                                            {pnl >= 0 ? '+' : ''}${pnl.toLocaleString('en-US', {minimumFractionDigits: 2})}
                                            &nbsp;({pnl >= 0 ? '+' : ''}{pnlPct.toFixed(2)}%)
                                        </strong>
                                    </span>
                                </div>
                            </div>

                            {/* Table + pie side by side */}
                            <div className="account-balances-body">
                                <div className="account-table">
                                    <div className="account-table-head">
                                        <span>Ticker</span>
                                        <span>Quantity</span>
                                        <span>Latest Price</span>
                                        <span>USD Value</span>
                                    </div>
                                    {selected.balances.map(({ticker, quantity}, i) => {
                                        const price = prices[ticker] ?? 0;
                                        const usdValue = quantity * price;
                                        return (
                                            <div className="account-row" key={ticker}>
                                                <span className="account-ticker"
                                                      style={{color: PIE_COLORS[i % PIE_COLORS.length]}}>
                                                    {ticker}
                                                </span>
                                                <span
                                                    className="account-value">{quantity.toLocaleString('en-US')}</span>
                                                <span className="account-value">${price.toFixed(4)}</span>
                                                <span className="account-value">
                                                    ${usdValue.toLocaleString('en-US', {minimumFractionDigits: 2})}
                                                </span>
                                            </div>
                                        );
                                    })}
                                </div>

                                {/* Pie chart */}
                                <div className="account-pie-wrapper">
                                    <ResponsiveContainer width="100%" height={260}>
                                        <PieChart>
                                            <Pie
                                                data={selected.balances.map(({ticker, quantity}) => ({
                                                    name: ticker,
                                                    value: parseFloat((quantity * (prices[ticker] ?? 0)).toFixed(2)),
                                                }))}
                                                cx="50%"
                                                cy="50%"
                                                innerRadius="52%"
                                                outerRadius="78%"
                                                paddingAngle={3}
                                                dataKey="value"
                                            >
                                                {selected.balances.map((_, i) => (
                                                    <Cell
                                                        key={i}
                                                        fill={PIE_COLORS[i % PIE_COLORS.length]}
                                                        stroke="transparent"
                                                    />
                                                ))}
                                            </Pie>
                                            <Tooltip
                                                formatter={(value) => [`$${value.toLocaleString('en-US', {minimumFractionDigits: 2})}`, 'USD Value']}
                                                contentStyle={{
                                                    backgroundColor: '#2a2a3a',
                                                    border: '1px solid #3a3a4a',
                                                    borderRadius: 8,
                                                    fontFamily: 'Poppins, sans-serif',
                                                    fontSize: 12,
                                                    color: '#e0e0f0',
                                                }}
                                                itemStyle={{color: '#e0e0f0'}}
                                                labelStyle={{color: '#a0a0b0'}}
                                            />
                                            <Legend
                                                iconType="circle"
                                                iconSize={8}
                                                formatter={(value) => (
                                                    <span style={{
                                                        fontFamily: 'Poppins, sans-serif',
                                                        fontSize: 12,
                                                        color: '#a0a0b0'
                                                    }}>
                                                        {value}
                                                    </span>
                                                )}
                                            />
                                        </PieChart>
                                    </ResponsiveContainer>
                                </div>
                            </div>
                        </section>
                    </>
                )}
            </main>

            {modalServer !== undefined && (
                <ServerModal server={modalServer} onClose={() => setModalServer(undefined)}/>
            )}
        </div>
    );
}

export default AccountPage;
