import React, {useState} from 'react';
import {useNavigate} from 'react-router-dom';
import './LobbyPage.css';

// TODO: fetch server list (including mdpEndpoint) from the database
const SERVERS = [
    {
        id: 1,
        name: 'FINA2303',
        admin: 'Prof. Wong',
        users: 24,
        description: 'Financial Markets & Institutions — Learn how exchanges, banks, and central banks shape global capital flows.',
        mdpEndpoint: 'ws://localhost:9001',
    },
    {
        id: 2,
        name: 'FINA3103',
        admin: 'Prof. Chen',
        users: 12,
        description: 'Intermediate Investments — Dive into portfolio theory, CAPM, and multi-factor models with live simulations.',
        mdpEndpoint: 'ws://localhost:9001',
    },
    {
        id: 3,
        name: 'FINA3203',
        admin: 'Prof. Li',
        users: 37,
        description: 'Derivative Securities — Price options, futures, and swaps using Black-Scholes and binomial trees.',
        mdpEndpoint: 'ws://localhost:9001',
    },
    {
        id: 4,
        name: 'FINA3303',
        admin: 'Prof. Zhang',
        users: 8,
        description: 'Fixed Income Analysis — Master bond pricing, duration, convexity, and yield-curve strategies.',
        mdpEndpoint: 'ws://localhost:9001',
    },
    {
        id: 5,
        name: 'FINA4103',
        admin: 'Prof. Lau',
        users: 19,
        description: 'Quantitative Trading Strategies — Develop and back-test algorithmic trading systems on real market data.',
        mdpEndpoint: 'ws://localhost:9001',
    },
    {
        id: 6,
        name: 'FINA4303',
        admin: 'Prof. Ho',
        users: 42,
        description: 'Risk Management — Measure and mitigate market, credit, and operational risk using VaR and stress tests.',
        mdpEndpoint: 'ws://localhost:9001',
    },
];

function LobbyPage() {
    const navigate = useNavigate();
    const [search, setSearch] = useState('');
    const [expandedId, setExpandedId] = useState(null);

    const filtered = SERVERS.filter(s =>
        s.name.toLowerCase().includes(search.toLowerCase()) ||
        s.admin.toLowerCase().includes(search.toLowerCase()) ||
        s.description.toLowerCase().includes(search.toLowerCase())
    );

    const handleRowClick = (id) => {
        setExpandedId(prev => (prev === id ? null : id));
    };

    const handleJoin = (e, server) => {
        e.stopPropagation();
        navigate(`/${server.name}/trading/AAPL`, {
            state: {mdpEndpoint: server.mdpEndpoint},
        });
    };

    return (
        <div className="lobby-page">
            <h1 className="lobby-title">Select a Server</h1>
            <p className="lobby-subtitle">Join a trading room to start practicing</p>

            <div className="lobby-search-wrapper">
                <span className="lobby-search-icon">🔍</span>
                <input
                    className="lobby-search-input"
                    type="text"
                    placeholder="Search by name, admin, or description…"
                    value={search}
                    onChange={e => setSearch(e.target.value)}
                />
                {search && (
                    <button className="lobby-search-clear" onClick={() => setSearch('')}>✕</button>
                )}
            </div>

            <div className="server-list">
                {filtered.length === 0 && (
                    <p className="server-list-empty">No servers match your search.</p>
                )}
                {filtered.map(server => {
                    const isOpen = expandedId === server.id;
                    return (
                        <div
                            key={server.id}
                            className={`server-row${isOpen ? ' expanded' : ''}`}
                            onClick={() => handleRowClick(server.id)}
                        >
                            <div className="server-row-main">
                                <div className="server-row-left">
                                    <span className="server-row-chevron">{isOpen ? '▾' : '▸'}</span>
                                    <span className="server-row-name">{server.name}</span>
                                    <span className="server-row-admin">{server.admin}</span>
                                    <span className="server-row-users">👥 {server.users} online</span>
                                </div>
                                <button
                                    className="server-row-join"
                                    onClick={e => handleJoin(e, server)}
                                >
                                    View
                                </button>
                            </div>
                            {isOpen && (
                                <div className="server-row-desc">
                                    {server.description}
                                </div>
                            )}
                        </div>
                    );
                })}
            </div>
        </div>
    );
}

export default LobbyPage;
