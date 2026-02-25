import React, {useState} from 'react';
import {useAuth} from '../context/AuthContext';
import './AccountPage.css';

// ---------------------------------------------------------------------------
// TODO: Replace with real DB queries.
//       fetchUserServers(username) → [{
//         id, name, description, users, mdpEndpoint, role ('admin'|'member'),
//         totalTrades, balances: [{ ticker, balance }]
//       }]
// ---------------------------------------------------------------------------
const DUMMY_SERVERS = [
    {
        id: 101,
        name: 'FINA4999',
        description: 'My personal quant lab for algo strategy testing.',
        users: 5,
        mdpEndpoint: 'ws://localhost:9000',
        role: 'admin',
        totalTrades: 312,
        balances: [
            {ticker: 'AAPL',  balance: 12500.00},
            {ticker: 'GOOGL', balance:  8340.50},
            {ticker: 'MSFT',  balance: 15200.75},
            {ticker: 'NVDA',  balance: 21000.00},
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
            {ticker: 'AMZN',  balance:  4100.00},
            {ticker: 'TSLA',  balance:  6780.25},
            {ticker: 'META',  balance:  3200.00},
            {ticker: 'JPM',   balance:  9450.80},
        ],
    },
    {
        id: 1,
        name: 'FINA2303',
        description: 'Financial Markets & Institutions — Prof. Wong.',
        users: 24,
        mdpEndpoint: 'ws://localhost:9000',
        role: 'member',
        totalTrades: 45,
        balances: [
            {ticker: 'AAPL', balance: 3200.00},
            {ticker: 'JPM',  balance: 1800.50},
        ],
    },
    {
        id: 3,
        name: 'FINA3203',
        description: 'Derivative Securities — Prof. Li.',
        users: 37,
        mdpEndpoint: 'ws://localhost:9000',
        role: 'member',
        totalTrades: 128,
        balances: [
            {ticker: 'TSLA', balance: 5600.00},
            {ticker: 'NVDA', balance: 4200.75},
            {ticker: 'META', balance: 2900.00},
        ],
    },
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
    const adminServers  = servers.filter((s) => s.role === 'admin');
    const memberServers = servers.filter((s) => s.role === 'member');
    const selected = servers.find((s) => s.id === selectedId) ?? null;
    const totalValue = selected
        ? selected.balances.reduce((sum, b) => sum + b.balance, 0)
        : 0;

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
                                <span className="account-panel-meta">
                                    Total&nbsp;
                                    <strong>${totalValue.toLocaleString('en-US', {minimumFractionDigits: 2})}</strong>
                                </span>
                            </div>
                            <div className="account-table">
                                <div className="account-table-head">
                                    <span>Ticker</span>
                                    <span>Balance (USD)</span>
                                </div>
                                {selected.balances.map(({ticker, balance}) => (
                                    <div className="account-row" key={ticker}>
                                        <span className="account-ticker">{ticker}</span>
                                        <span className="account-value">
                                            ${balance.toLocaleString('en-US', {minimumFractionDigits: 2})}
                                        </span>
                                    </div>
                                ))}
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
