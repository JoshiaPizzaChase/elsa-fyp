import React, {useState, useEffect} from 'react';
import {useNavigate} from 'react-router-dom';
import {getActiveServers} from '../api';
import './LobbyPage.css';

function LobbyPage() {
    const navigate = useNavigate();
    const [search, setSearch] = useState('');
    const [expandedId, setExpandedId] = useState(null);
    const [servers, setServers] = useState([]);
    const [loading, setLoading] = useState(true);

    useEffect(() => {
        getActiveServers()
            .then((data) => setServers(data.servers ?? []))
            .catch((err) => console.error('Failed to fetch servers:', err))
            .finally(() => setLoading(false));
    }, []);

    const filtered = servers.filter(s =>
        (s.server_name ?? '').toLowerCase().includes(search.toLowerCase()) ||
        (s.admin_name ?? '').toLowerCase().includes(search.toLowerCase())
    );

    const handleRowClick = (id) => {
        setExpandedId(prev => (prev === id ? null : id));
    };

    const handleJoin = (e, server) => {
        e.stopPropagation();
        const mdpEndpoint = (server.mdp_ip && server.mdp_port)
            ? `ws://${server.mdp_ip}:${server.mdp_port}`
            : '';
        const firstSymbol = (server.active_symbols?.[0] ?? 'AAPL').toUpperCase();
        navigate(`/${server.server_name}/trading/${firstSymbol}`, {state: {mdpEndpoint}});
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
                    placeholder="Search by name or admin…"
                    value={search}
                    onChange={e => setSearch(e.target.value)}
                />
                {search && (
                    <button className="lobby-search-clear" onClick={() => setSearch('')}>✕</button>
                )}
            </div>

            <div className="server-list">
                {loading && <p className="server-list-empty">Loading servers…</p>}
                {!loading && filtered.length === 0 && (
                    <p className="server-list-empty">No servers match your search.</p>
                )}
                {filtered.map(server => {
                    const isOpen = expandedId === server.server_id;
                    return (
                        <div
                            key={server.server_id}
                            className={`server-row${isOpen ? ' expanded' : ''}`}
                            onClick={() => handleRowClick(server.server_id)}
                        >
                            <div className="server-row-main">
                                <div className="server-row-left">
                                    <span className="server-row-chevron">{isOpen ? '▾' : '▸'}</span>
                                    <span className="server-row-name">{server.server_name}</span>
                                    <span className="server-row-admin">{server.admin_name}</span>
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
                                    {server.description && (
                                        <span className="server-row-desc-text">{server.description}</span>
                                    )}
                                    <span className="server-row-desc-symbols">
                                        Symbols: {(server.active_symbols ?? []).join(', ') || 'None'}
                                    </span>
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
