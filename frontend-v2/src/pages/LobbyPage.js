import React from 'react';
import {useNavigate} from 'react-router-dom';
import './LobbyPage.css';

const SERVERS = [
    {
        id: 1,
        name: 'FINA2303',
        admin: 'Prof. Wong',
        users: 24,
        description: 'Financial Markets & Institutions — Learn how exchanges, banks, and central banks shape global capital flows.'
    },
    {
        id: 2,
        name: 'FINA3103',
        admin: 'Prof. Chen',
        users: 12,
        description: 'Intermediate Investments — Dive into portfolio theory, CAPM, and multi-factor models with live simulations.'
    },
    {
        id: 3,
        name: 'FINA3203',
        admin: 'Prof. Li',
        users: 37,
        description: 'Derivative Securities — Price options, futures, and swaps using Black-Scholes and binomial trees.'
    },
    {
        id: 4,
        name: 'FINA3303',
        admin: 'Prof. Zhang',
        users: 8,
        description: 'Fixed Income Analysis — Master bond pricing, duration, convexity, and yield-curve strategies.'
    },
    {
        id: 5,
        name: 'FINA4103',
        admin: 'Prof. Lau',
        users: 19,
        description: 'Quantitative Trading Strategies — Develop and back-test algorithmic trading systems on real market data.'
    },
    {
        id: 6,
        name: 'FINA4303',
        admin: 'Prof. Ho',
        users: 42,
        description: 'Risk Management — Measure and mitigate market, credit, and operational risk using VaR and stress tests.'
    },
];

function LobbyPage() {
    const navigate = useNavigate();

    const handleServerClick = (serverName) => {
        navigate(`/${serverName}/trading/AAPL`);
    };

    return (
        <div className="lobby-page">
            <h1 className="lobby-title">Select a Server</h1>
            <p className="lobby-subtitle">Join a trading room to start practicing</p>
            <div className="server-grid">
                {SERVERS.map(server => (
                    <div
                        key={server.id}
                        className="server-card"
                        onClick={() => handleServerClick(server.name)}
                    >
                        <div className="server-card-name">{server.name}</div>
                        <div className="server-card-description">{server.description}</div>
                        <div className="server-card-info">
                            <span className="server-card-label">Admin</span>
                            <span className="server-card-value">{server.admin}</span>
                        </div>
                        <div className="server-card-info">
                            <span className="server-card-label">Users</span>
                            <span className="server-card-value">{server.users} online</span>
                        </div>
                    </div>
                ))}
            </div>
        </div>
    );
}

export default LobbyPage;
