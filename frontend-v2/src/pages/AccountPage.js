import React, {useState, useEffect} from 'react';
import {useAuth} from '../context/AuthContext';
import {getUserServers} from '../api';
import {PieChart, Pie, Cell, Tooltip, ResponsiveContainer, Legend} from 'recharts';
import './AccountPage.css';

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
    const [servers, setServers] = useState([]);
    const [selectedId, setSelectedId] = useState(null);
    const [modalServer, setModalServer] = useState(undefined);
    const [loading, setLoading] = useState(true);

    useEffect(() => {
        if (!user?.username) return;
        setLoading(true);
        getUserServers(user.username)
            .then((data) => {
                const list = data.servers ?? [];
                setServers(list);
                if (list.length > 0) setSelectedId(list[0].server_id);
            })
            .catch((err) => console.error('Failed to fetch user servers:', err))
            .finally(() => setLoading(false));
    }, [user?.username]);

    const adminServers = servers.filter((s) => s.role === 'admin');
    const memberServers = servers.filter((s) => s.role === 'member');
    const selected = servers.find((s) => s.server_id === selectedId) ?? null;
    const balances = selected?.balances ?? [];
    const totalValue = balances.reduce((sum, b) => sum + (b.balance ?? 0), 0);
    const pnl = totalValue - INITIAL_BALANCE;
    const pnlPct = INITIAL_BALANCE !== 0 ? (pnl / INITIAL_BALANCE) * 100 : 0;

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
                            key={srv.server_id}
                            className={`account-nav-item${selectedId === srv.server_id ? ' active' : ''}`}
                            onClick={() => setSelectedId(srv.server_id)}
                        >
                            <span className="account-nav-name">{srv.server_name}</span>
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
                            key={srv.server_id}
                            className={`account-nav-item${selectedId === srv.server_id ? ' active' : ''}`}
                            onClick={() => setSelectedId(srv.server_id)}
                        >
                            <span className="account-nav-name">{srv.server_name}</span>
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
                                <h2 className="account-detail-name">{selected.server_name}</h2>
                                <span className={`account-detail-badge ${selected.role}`}>
                                    {selected.role === 'admin' ? 'Admin' : 'Member'}
                                </span>
                            </div>
                            <p className="account-detail-desc">
                                Symbols: {(selected.active_symbols ?? []).join(', ') || 'None'}
                            </p>
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
                                <span className="account-stat-value">{balances.length}</span>
                                <span className="account-stat-label">Positions</span>
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
                                        <span>Symbol</span>
                                        <span>Balance</span>
                                    </div>
                                    {balances.map(({symbol, balance}, i) => {
                                        return (
                                            <div className="account-row" key={symbol}>
                                                <span className="account-ticker"
                                                      style={{color: PIE_COLORS[i % PIE_COLORS.length]}}>
                                                    {symbol}
                                                </span>
                                                <span
                                                    className="account-value">{balance.toLocaleString('en-US')}</span>
                                            </div>
                                        );
                                    })}
                                </div>

                                {/* Pie chart */}
                                <div className="account-pie-wrapper">
                                    <ResponsiveContainer width="100%" height={260}>
                                        <PieChart>
                                            <Pie
                                                data={balances.map(({symbol, balance}) => ({
                                                    name: symbol,
                                                    value: balance,
                                                }))}
                                                cx="50%"
                                                cy="50%"
                                                innerRadius="52%"
                                                outerRadius="78%"
                                                paddingAngle={3}
                                                dataKey="value"
                                            >
                                                {balances.map((_, i) => (
                                                    <Cell
                                                        key={i}
                                                        fill={PIE_COLORS[i % PIE_COLORS.length]}
                                                        stroke="transparent"
                                                    />
                                                ))}
                                            </Pie>
                                            <Tooltip
                                                formatter={(value) => [value.toLocaleString('en-US'), 'Balance']}
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
