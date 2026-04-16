import React, {useState, useEffect} from 'react';
import {useAuth} from '../context/AuthContext';
import {
    getUserServers,
    getAccountDetails,
    createServer,
    configureServer,
    removeServer,
} from '../api';
import {PieChart, Pie, Cell, Tooltip, ResponsiveContainer, Legend} from 'recharts';
import './AccountPage.css';

const DEFAULT_INITIAL_USD = 100000;
const SERVER_NAME_MAX_LENGTH = 11;
const BOT_TYPES = ['mm', 'nt', 'it'];
const BOT_META = {
    mm: {label: 'Market Maker', shortLabel: 'MM', countLabel: 'Market Makers'},
    nt: {label: 'Noise Trader', shortLabel: 'NT', countLabel: 'Noise Traders'},
    it: {label: 'Informed Trader', shortLabel: 'IT', countLabel: 'Informed Traders'},
};
const DEFAULT_BOT_GROUP = {
    count: 0,
    initial_usd: DEFAULT_INITIAL_USD,
    initial_inventory: 100,
    inventory_by_symbol: {},
    params: {},
};
const DEFAULT_BOT_CONFIG = {
    mm: {
        groups: [{
            ...DEFAULT_BOT_GROUP,
            count: 4,
            initial_usd: 300000,
            initial_inventory: 300,
            params: {lot_size: 1, gamma: 0.6, k: 8, terminal_time: 30},
        }],
    },
    nt: {
        groups: [{
            ...DEFAULT_BOT_GROUP,
            count: 4,
            initial_usd: 80000,
            initial_inventory: 20,
            params: {lambda_eps: 0.6, bernoulli: 0.5, pareto_scale: 0.5, pareto_shape: 4},
        }],
    },
    it: {
        groups: [{
            ...DEFAULT_BOT_GROUP,
            count: 1,
            initial_usd: 1500000,
            initial_inventory: 150,
            params: {epsilon: 0.12, trade_qty: 5, max_inventory: 1200},
        }],
    },
};
const DEFAULT_ORACLE_CONFIG = {
    mu: 0.0,
    sigma: 0.003,
    jump_intensity: 0.06,
    jump_mean: 0.0,
    jump_std: 0.01,
    update_interval_ms: 60000,
};

const createDefaultBotConfig = () => JSON.parse(JSON.stringify(DEFAULT_BOT_CONFIG));

const PIE_COLORS = [
    '#a29bfe', '#6c5ce7', '#74b9ff', '#0984e3',
    '#55efc4', '#00b894', '#ffeaa7', '#fdcb6e',
];

// ── TagInput ──────────────────────────────────────────────────────────────────
// A small reusable tag-input: split by comma/space/Enter, click × to remove.
function TagInput({label, tags, onChange, placeholder}) {
    const [draft, setDraft] = useState('');

    const addTags = (raw) => {
        const parsed = raw
            .split(/[,\s]+/)
            .map(v => v.trim().toUpperCase())
            .filter(Boolean);
        if (parsed.length === 0) return;

        const existing = new Set(tags);
        const next = [...tags];
        for (const val of parsed) {
            if (!existing.has(val)) {
                existing.add(val);
                next.push(val);
            }
        }
        if (next.length !== tags.length) onChange(next);
    };

    const handleKey = (e) => {
        if (e.key === 'Enter' || e.key === ',' || e.key === ' ') {
            e.preventDefault();
            addTags(draft);
            setDraft('');
        } else if (e.key === 'Backspace' && draft === '' && tags.length > 0) {
            onChange(tags.slice(0, -1));
        }
    };

    const handleBlur = () => {
        if (draft.trim()) {
            addTags(draft);
            setDraft('');
        }
    };

    return (
        <div className="sm-field">
            <label className="sm-label">{label}</label>
            <div className="sm-tag-box">
                {tags.map(t => (
                    <span key={t} className="sm-tag">
                        {t}
                        <button type="button" className="sm-tag-remove"
                                onClick={() => onChange(tags.filter(x => x !== t))}>×</button>
                    </span>
                ))}
                <input
                    className="sm-tag-input"
                    value={draft}
                    onChange={e => {
                        const nextDraft = e.target.value;
                        if (/[,\s]/.test(nextDraft)) {
                            addTags(nextDraft);
                            setDraft('');
                            return;
                        }
                        setDraft(nextDraft);
                    }}
                    onKeyDown={handleKey}
                    onBlur={handleBlur}
                    placeholder={tags.length === 0 ? placeholder : ''}
                />
            </div>
        </div>
    );
}

// ── AllowlistInput ────────────────────────────────────────────────────────────
// Same as TagInput but preserves original casing (usernames are case-sensitive).
function AllowlistInput({label, tags, onChange, placeholder}) {
    const [draft, setDraft] = useState('');

    const addTags = (raw) => {
        const parsed = raw
            .split(/[,\s]+/)
            .map(v => v.trim())
            .filter(Boolean);
        if (parsed.length === 0) return;

        const existing = new Set(tags);
        const next = [...tags];
        for (const val of parsed) {
            if (!existing.has(val)) {
                existing.add(val);
                next.push(val);
            }
        }
        if (next.length !== tags.length) onChange(next);
    };

    const handleKey = (e) => {
        if (e.key === 'Enter' || e.key === ',' || e.key === ' ') {
            e.preventDefault();
            addTags(draft);
            setDraft('');
        } else if (e.key === 'Backspace' && draft === '' && tags.length > 0) {
            onChange(tags.slice(0, -1));
        }
    };

    const handleBlur = () => {
        if (draft.trim()) {
            addTags(draft);
            setDraft('');
        }
    };

    return (
        <div className="sm-field">
            <label className="sm-label">{label}</label>
            <div className="sm-tag-box">
                {tags.map(t => (
                    <span key={t} className="sm-tag sm-tag-user">
                        {t}
                        <button type="button" className="sm-tag-remove"
                                onClick={() => onChange(tags.filter(x => x !== t))}>×</button>
                    </span>
                ))}
                <input
                    className="sm-tag-input"
                    value={draft}
                    onChange={e => {
                        const nextDraft = e.target.value;
                        if (/[,\s]/.test(nextDraft)) {
                            addTags(nextDraft);
                            setDraft('');
                            return;
                        }
                        setDraft(nextDraft);
                    }}
                    onKeyDown={handleKey}
                    onBlur={handleBlur}
                    placeholder={tags.length === 0 ? placeholder : ''}
                />
            </div>
        </div>
    );
}

function BotSettingsModal({botType, config, symbols, onGroupChange, onAddGroup, onRemoveGroup, onClose}) {
    if (!botType || !config) return null;
    const botMeta = BOT_META[botType];
    const groups = config.groups ?? [];
    const [openGroupIndex, setOpenGroupIndex] = useState(0);

    useEffect(() => {
        setOpenGroupIndex(0);
    }, [botType, groups.length]);

    return (
        <div className="sm-bot-settings-overlay" onClick={onClose}>
            <div className="sm-bot-settings-modal" onClick={e => e.stopPropagation()}>
                <div className="sm-bot-settings-header">
                    <h3 className="sm-bot-settings-title">{botMeta.label} Settings</h3>
                    <button type="button" className="sm-x-btn" onClick={onClose} title="Close">✕</button>
                </div>

                <div className="sm-bot-groups-header">
                    <span className="sm-label">Groups</span>
                    <button type="button" className="sm-save-btn" onClick={() => onAddGroup(botType)}>
                        + Add Group
                    </button>
                </div>

                <div className="sm-bot-groups-list">
                    {groups.map((group, groupIndex) => {
                        const paramEntries = Object.entries(group.params ?? {});
                        const isOpen = openGroupIndex === groupIndex;
                        return (
                            <div className="sm-bot-group-card" key={`grp-${groupIndex}`}>
                                <div className="sm-bot-group-card-header">
                                    <button
                                        type="button"
                                        className="sm-bot-group-toggle-btn"
                                        onClick={() => setOpenGroupIndex(isOpen ? -1 : groupIndex)}
                                    >
                                        <span className="sm-bot-group-title">{`Group ${groupIndex + 1}`}</span>
                                        <span className="sm-bot-group-meta">
                                            {`${Number(group.count) || 0} bots ${isOpen ? '▾' : '▸'}`}
                                        </span>
                                    </button>
                                    <button
                                        type="button"
                                        className="sm-cancel-btn"
                                        disabled={groups.length <= 1}
                                        onClick={() => onRemoveGroup(botType, groupIndex)}
                                    >
                                        Remove
                                    </button>
                                </div>
                                {isOpen && (
                                    <div className="sm-bot-settings-grid">
                                        <div className="sm-field">
                                            <label className="sm-label">Bot Count</label>
                                            <input
                                                className="sm-input"
                                                type="number"
                                                min="0"
                                                step="1"
                                                value={group.count}
                                                onChange={e => onGroupChange(botType, groupIndex, 'count', e.target.value)}
                                            />
                                        </div>
                                        <div className="sm-field">
                                            <label className="sm-label">Initial USD</label>
                                            <input
                                                className="sm-input"
                                                type="number"
                                                min="0"
                                                step="1"
                                                value={group.initial_usd}
                                                onChange={e => onGroupChange(botType, groupIndex, 'initial_usd', e.target.value)}
                                            />
                                        </div>
                                        {symbols.map((symbol) => (
                                            <div className="sm-field" key={`inv-${groupIndex}-${symbol}`}>
                                                <label className="sm-label">Inventory ({symbol})</label>
                                                <input
                                                    className="sm-input"
                                                    type="number"
                                                    min="0"
                                                    step="0.01"
                                                    value={group.inventory_by_symbol?.[symbol] ?? 0}
                                                    onChange={e => onGroupChange(botType, groupIndex, `inventory_by_symbol.${symbol}`, e.target.value)}
                                                />
                                            </div>
                                        ))}
                                        {paramEntries.map(([paramKey, paramValue]) => (
                                            <div className="sm-field" key={`${groupIndex}-${paramKey}`}>
                                                <label className="sm-label">{paramKey.replaceAll('_', ' ')}</label>
                                                <input
                                                    className="sm-input"
                                                    type="number"
                                                    min="0"
                                                    step="0.01"
                                                    value={paramValue}
                                                    onChange={e => onGroupChange(botType, groupIndex, `params.${paramKey}`, e.target.value)}
                                                />
                                            </div>
                                        ))}
                                    </div>
                                )}
                            </div>
                        );
                    })}
                </div>

                <div className="sm-actions">
                    <button type="button" className="sm-save-btn" onClick={onClose}>Done</button>
                </div>
            </div>
        </div>
    );
}

function BotCard({botType, config, onOpenSettings}) {
    const botMeta = BOT_META[botType];
    const totalBots = (config.groups ?? []).reduce((sum, grp) => sum + (Number(grp.count) || 0), 0);
    return (
        <div className="sm-bot-card">
            <div className="sm-bot-card-header">
                <div>
                    <div className="sm-bot-card-title">{botMeta.label}</div>
                    <div className="sm-bot-card-subtitle">{botMeta.shortLabel}</div>
                </div>
                <button
                    type="button"
                    className="sm-bot-settings-btn"
                    onClick={() => onOpenSettings(botType)}
                    title={`Configure ${botMeta.label}`}
                    aria-label={`Configure ${botMeta.label}`}
                >
                    ⚙
                </button>
            </div>
            <div className="sm-hint">{`${config.groups?.length ?? 0} group(s), ${totalBots} total`}</div>
        </div>
    );
}

// ── ServerModal ───────────────────────────────────────────────────────────────
function ServerModal({server, username, onClose, onSaved}) {
    const isNew = server === null;
    const getServerNameError = (value) => {
        if (!value) return '';
        if (value.length > SERVER_NAME_MAX_LENGTH) {
            return 'Server name must be less than 12 characters.';
        }
        if (!/^[A-Za-z0-9]+$/.test(value)) {
            return 'Server name can only contain A-Z, a-z, and 0-9.';
        }
        return '';
    };

    // Pre-populate fields when editing
    const [serverName, setServerName] = useState(isNew ? '' : (server.server_name ?? ''));
    const [description, setDescription] = useState(isNew ? '' : (server.description ?? ''));
    const [symbols, setSymbols] = useState(isNew ? [] : (server.active_symbols ?? []));
    // Allowlist: for editing we'd need to fetch it; pre-fill with empty for now
    const [allowlist, setAllowlist] = useState([]);
    const [initialUsd, setInitialUsd] = useState(
        isNew ? DEFAULT_INITIAL_USD : (server.initial_usd ?? DEFAULT_INITIAL_USD)
    );
    const [botConfig, setBotConfig] = useState(createDefaultBotConfig());
    const [oracleConfig, setOracleConfig] = useState({...DEFAULT_ORACLE_CONFIG});
    const [activeBotSettings, setActiveBotSettings] = useState(null);

    const [saving, setSaving] = useState(false);
    const [toast, setToast] = useState(null); // {type:'success'|'error', msg}
    const serverNameError = isNew ? getServerNameError(serverName) : '';

    const showToast = (type, msg) => {
        setToast({type, msg});
        setTimeout(() => setToast(null), 4000);
    };

    useEffect(() => {
        if (!isNew) return;
        setBotConfig(prev => {
            const next = {...prev};
            BOT_TYPES.forEach((botType) => {
                const current = next[botType] ?? createDefaultBotConfig()[botType];
                next[botType] = {
                    ...current,
                    groups: (current.groups ?? []).map((group) => {
                        const nextInventory = {};
                        symbols.forEach((symbol) => {
                            nextInventory[symbol] =
                                group.inventory_by_symbol?.[symbol] ?? group.initial_inventory ?? 0;
                        });
                        return {
                            ...group,
                            inventory_by_symbol: nextInventory,
                        };
                    }),
                };
            });
            return next;
        });
    }, [symbols, isNew]);

    const handleSubmit = async (e) => {
        e.preventDefault();
        if (!serverName.trim()) {
            showToast('error', 'Server name is required.');
            return;
        }
        if (serverNameError) {
            showToast('error', serverNameError);
            return;
        }

        setSaving(true);
        const normalizedBots = BOT_TYPES.reduce((acc, botType) => {
            const current = botConfig[botType];
            const groups = (current.groups ?? []).map((group) => {
                const normalizedGroup = {
                    count: Number.isFinite(Number(group.count)) ? Math.max(0, Math.floor(Number(group.count))) : 0,
                    initial_usd: Number.isFinite(Number(group.initial_usd))
                        ? Math.max(0, Math.floor(Number(group.initial_usd)))
                        : DEFAULT_INITIAL_USD,
                    initial_inventory: 0,
                    inventory_by_symbol: {},
                    params: {},
                };
                const symbolInventories = symbols.reduce((inv, symbol) => {
                    const rawInventory = group.inventory_by_symbol?.[symbol];
                    inv[symbol] = Number.isFinite(Number(rawInventory)) ? Math.max(0, Number(rawInventory)) : 0;
                    return inv;
                }, {});
                normalizedGroup.inventory_by_symbol = symbolInventories;
                const firstSymbol = symbols.find((symbol) => symbol !== 'USD') ?? symbols[0];
                normalizedGroup.initial_inventory = firstSymbol
                    ? (symbolInventories[firstSymbol] ?? 0)
                    : 0;
                Object.entries(group.params ?? {}).forEach(([key, value]) => {
                    normalizedGroup.params[key] = Number.isFinite(Number(value)) ? Math.max(0, Number(value)) : 0;
                });
                return normalizedGroup;
            });
            const normalized = {groups};
            acc[botType] = normalized;
            return acc;
        }, {});
        const payload = {
            server_name: serverName,
            description: description.trim(),
            active_symbols: symbols,
            allowlist,
            initial_usd: Number.isFinite(Number(initialUsd)) ? Number(initialUsd) : DEFAULT_INITIAL_USD,
            oracle: {
                mu: Number.isFinite(Number(oracleConfig.mu)) ? Number(oracleConfig.mu) : DEFAULT_ORACLE_CONFIG.mu,
                sigma: Number.isFinite(Number(oracleConfig.sigma)) ? Math.max(0, Number(oracleConfig.sigma)) : DEFAULT_ORACLE_CONFIG.sigma,
                jump_intensity: Number.isFinite(Number(oracleConfig.jump_intensity))
                    ? Math.max(0, Number(oracleConfig.jump_intensity))
                    : DEFAULT_ORACLE_CONFIG.jump_intensity,
                jump_mean: Number.isFinite(Number(oracleConfig.jump_mean))
                    ? Number(oracleConfig.jump_mean)
                    : DEFAULT_ORACLE_CONFIG.jump_mean,
                jump_std: Number.isFinite(Number(oracleConfig.jump_std))
                    ? Math.max(0, Number(oracleConfig.jump_std))
                    : DEFAULT_ORACLE_CONFIG.jump_std,
                update_interval_ms: Number.isFinite(Number(oracleConfig.update_interval_ms))
                    ? Math.max(1, Math.floor(Number(oracleConfig.update_interval_ms)))
                    : DEFAULT_ORACLE_CONFIG.update_interval_ms,
            },
            ...(isNew ? {bots: normalizedBots} : {}),
        };

        try {
            let data;
            if (isNew) {
                data = await createServer(username, payload);
            } else {
                data = await configureServer(username, payload);
            }

            if (data.auth_error) {
                showToast('error', data.auth_error);
            } else if (data.error) {
                showToast('error', data.error);
            } else {
                showToast('success', isNew
                    ? `Server "${data.server_name}" created!`
                    : `Server "${data.server_name}" updated!`);
                if (onSaved) onSaved(data);
                // Close after a brief moment so the toast is visible
                setTimeout(onClose, 1500);
            }
        } catch (err) {
            const msg = err?.data?.auth_error ?? err?.data?.error ?? err.message ?? 'Request failed';
            showToast('error', msg);
        } finally {
            setSaving(false);
        }
    };

    return (
        <div className="modal-overlay" onClick={onClose}>
            <div className="modal-box sm-box" onClick={e => e.stopPropagation()}>

                {/* Header */}
                <div className="sm-header">
                    <h2 className="modal-title">
                        {isNew ? '+ Create Server' : `Edit — ${server.server_name}`}
                    </h2>
                    <button className="sm-x-btn" onClick={onClose} title="Close">✕</button>
                </div>

                {/* Auth notice */}
                <p className="sm-auth-notice">
                    🔐 Only the server admin can save changes.
                    Your identity is sent as <code>Authorization: Bearer {username}</code>.
                </p>

                {/* Toast */}
                {toast && (
                    <div className={`sm-toast sm-toast-${toast.type}`}>{toast.msg}</div>
                )}

                <form className="sm-form" onSubmit={handleSubmit}>

                    {/* Server name — locked when editing */}
                    <div className="sm-field">
                        <label className="sm-label">Server Name <span className="sm-required">*</span></label>
                        <input
                            className="sm-input"
                            value={serverName}
                            onChange={e => setServerName(e.target.value)}
                            placeholder="e.g. hk01"
                            pattern="[A-Za-z0-9]{1,11}"
                            title="1-11 characters, only A-Z, a-z, 0-9"
                            aria-invalid={serverNameError ? 'true' : 'false'}
                            disabled={!isNew}
                            required
                        />
                        {isNew && serverNameError && (
                            <span className="sm-hint sm-hint-error">{serverNameError}</span>
                        )}
                        {!isNew && (
                            <span className="sm-hint">Server name cannot be changed after creation.</span>
                        )}
                    </div>

                    {/* Description */}
                    <div className="sm-field">
                        <label className="sm-label">Description</label>
                        <textarea
                            className="sm-input sm-textarea"
                            value={description}
                            onChange={e => setDescription(e.target.value)}
                            placeholder="Short description shown to users"
                            rows={2}
                        />
                    </div>

                    {isNew && (
                        <div className="sm-field">
                            <label className="sm-label">Initial USD per Member</label>
                            <input
                                className="sm-input"
                                type="number"
                                min="0"
                                step="1"
                                value={initialUsd}
                                onChange={e => setInitialUsd(e.target.value)}
                                placeholder="e.g. 100000"
                                required
                            />
                        </div>
                    )}

                    {/* Active symbols */}
                    <TagInput
                        label="Active Symbols"
                        tags={symbols}
                        onChange={setSymbols}
                        placeholder="Type symbols separated by space or comma…"
                    />

                    {isNew && (
                        <div className="sm-field">
                            <label className="sm-label">Oracle Service</label>
                            <div className="sm-bot-settings-grid">
                                <div className="sm-field">
                                    <label className="sm-label">mu</label>
                                    <input
                                        className="sm-input"
                                        type="number"
                                        step="0.001"
                                        value={oracleConfig.mu}
                                        onChange={e => setOracleConfig(prev => ({...prev, mu: e.target.value}))}
                                    />
                                </div>
                                <div className="sm-field">
                                    <label className="sm-label">sigma</label>
                                    <input
                                        className="sm-input"
                                        type="number"
                                        min="0"
                                        step="0.001"
                                        value={oracleConfig.sigma}
                                        onChange={e => setOracleConfig(prev => ({...prev, sigma: e.target.value}))}
                                    />
                                </div>
                                <div className="sm-field">
                                    <label className="sm-label">jump intensity</label>
                                    <input
                                        className="sm-input"
                                        type="number"
                                        min="0"
                                        step="0.001"
                                        value={oracleConfig.jump_intensity}
                                        onChange={e => setOracleConfig(prev => ({...prev, jump_intensity: e.target.value}))}
                                    />
                                </div>
                                <div className="sm-field">
                                    <label className="sm-label">jump mean</label>
                                    <input
                                        className="sm-input"
                                        type="number"
                                        step="0.001"
                                        value={oracleConfig.jump_mean}
                                        onChange={e => setOracleConfig(prev => ({...prev, jump_mean: e.target.value}))}
                                    />
                                </div>
                                <div className="sm-field">
                                    <label className="sm-label">jump std</label>
                                    <input
                                        className="sm-input"
                                        type="number"
                                        min="0"
                                        step="0.001"
                                        value={oracleConfig.jump_std}
                                        onChange={e => setOracleConfig(prev => ({...prev, jump_std: e.target.value}))}
                                    />
                                </div>
                                <div className="sm-field">
                                    <label className="sm-label">update interval (ms)</label>
                                    <input
                                        className="sm-input"
                                        type="number"
                                        min="1"
                                        step="1"
                                        value={oracleConfig.update_interval_ms}
                                        onChange={e => setOracleConfig(prev => ({...prev, update_interval_ms: e.target.value}))}
                                    />
                                </div>
                            </div>
                        </div>
                    )}

                    {isNew && (
                        <div className="sm-field">
                            <label className="sm-label">Simulation Bots</label>
                            <div className="sm-bot-grid">
                                {BOT_TYPES.map((botType) => (
                                    <BotCard
                                        key={botType}
                                        botType={botType}
                                        config={botConfig[botType]}
                                        onOpenSettings={setActiveBotSettings}
                                    />
                                ))}
                            </div>
                        </div>
                    )}

                    {/* Allowlist */}
                    <AllowlistInput
                        label="Allowed Users (Allowlist)"
                        tags={allowlist}
                        onChange={setAllowlist}
                        placeholder="Type usernames separated by space or comma…"
                    />

                    {/* Actions */}
                    <div className="sm-actions">
                        <button type="button" className="sm-cancel-btn" onClick={onClose}>
                            Cancel
                        </button>
                        <button type="submit" className="sm-save-btn" disabled={saving}>
                            {saving ? 'Saving…' : (isNew ? 'Create Server' : 'Save Changes')}
                        </button>
                    </div>
                </form>
            </div>
            {isNew && (
                <BotSettingsModal
                    botType={activeBotSettings}
                    config={activeBotSettings ? botConfig[activeBotSettings] : null}
                    symbols={symbols}
                    onAddGroup={(targetType) => {
                        setBotConfig(prev => {
                            const next = {...prev};
                            const target = {...next[targetType]};
                            const templateGroup = target.groups?.[0] ?? {...DEFAULT_BOT_GROUP};
                            const newInventory = {};
                            symbols.forEach((symbol) => {
                                newInventory[symbol] = templateGroup.inventory_by_symbol?.[symbol]
                                    ?? templateGroup.initial_inventory
                                    ?? 0;
                            });
                            const newGroup = {
                                ...templateGroup,
                                count: 0,
                                inventory_by_symbol: newInventory,
                            };
                            target.groups = [...(target.groups ?? []), newGroup];
                            next[targetType] = target;
                            return next;
                        });
                    }}
                    onRemoveGroup={(targetType, groupIndex) => {
                        setBotConfig(prev => {
                            const next = {...prev};
                            const target = {...next[targetType]};
                            const groups = [...(target.groups ?? [])];
                            if (groups.length <= 1) return prev;
                            groups.splice(groupIndex, 1);
                            target.groups = groups;
                            next[targetType] = target;
                            return next;
                        });
                    }}
                    onClose={() => setActiveBotSettings(null)}
                    onGroupChange={(targetType, groupIndex, keyPath, rawValue) => {
                        setBotConfig(prev => {
                            const next = {...prev};
                            const target = {...next[targetType]};
                            const groups = [...(target.groups ?? [])];
                            if (!groups[groupIndex]) return prev;
                            const group = {...groups[groupIndex]};
                            if (keyPath.startsWith('params.')) {
                                const paramKey = keyPath.slice('params.'.length);
                                group.params = {...group.params, [paramKey]: rawValue};
                            } else if (keyPath.startsWith('inventory_by_symbol.')) {
                                const symbolKey = keyPath.slice('inventory_by_symbol.'.length);
                                group.inventory_by_symbol = {
                                    ...(group.inventory_by_symbol ?? {}),
                                    [symbolKey]: rawValue,
                                };
                            } else {
                                group[keyPath] = rawValue;
                            }
                            groups[groupIndex] = group;
                            target.groups = groups;
                            next[targetType] = target;
                            return next;
                        });
                    }}
                />
            )}
        </div>
    );
}

function AccountPage() {
    const {user} = useAuth();
    const [servers, setServers] = useState([]);
    const [selectedId, setSelectedId] = useState(null);
    const [modalServer, setModalServer] = useState(undefined);
    const [loading, setLoading] = useState(true);
    const [accountDetails, setAccountDetails] = useState(null);
    const [detailsLoading, setDetailsLoading] = useState(false);
    const [removeLoading, setRemoveLoading] = useState(false);

    const refreshServers = () => {
        if (!user?.username) return;
        setLoading(true);
        getUserServers(user.username)
            .then((data) => {
                const list = data.servers ?? [];
                setServers(list);
                if (list.length > 0) setSelectedId(list[0].server_id);
                else setSelectedId(null);
            })
            .catch((err) => console.error('Failed to fetch user servers:', err))
            .finally(() => setLoading(false));
    };

    useEffect(() => {
        refreshServers();
    }, [user?.username]);

    const selectedServer = servers.find((s) => s.server_id === selectedId) ?? null;

    useEffect(() => {
        if (!user?.username || !selectedId || servers.length === 0) {
            setAccountDetails(null);
            return;
        }
        const srv = servers.find((s) => s.server_id === selectedId);
        if (!srv) return;
        setDetailsLoading(true);
        setAccountDetails(null);
        getAccountDetails(user.username, srv.server_name)
            .then((data) => setAccountDetails(data))
            .catch((err) => {
                console.error('Failed to fetch account details:', err);
                setAccountDetails(null);
            })
            .finally(() => setDetailsLoading(false));
    }, [user?.username, selectedId, servers]);

    const adminServers = servers.filter((s) => s.role === 'admin');
    const memberServers = servers.filter((s) => s.role === 'member');
    // Merge server list data with the richer account details response.
    // accountDetails shape: { server: {...}, role, balances, total_value, pnl, pnl_pct }
    // Hoist accountDetails.server one level up so selected.active_symbols etc. work directly.
    const selected = selectedServer
        ? {...selectedServer, ...(accountDetails?.server ?? {}), ...(accountDetails ?? {})}
        : null;
    const balances = accountDetails?.balances ?? selected?.balances ?? [];
    const totalValue = accountDetails?.total_value
        ?? balances.reduce((sum, b) => sum + (b.balance ?? 0), 0);
    const initialUsd = accountDetails?.initial_usd
        ?? selected?.initial_usd
        ?? selected?.server?.initial_usd
        ?? DEFAULT_INITIAL_USD;
    const pnl = accountDetails?.pnl ?? (totalValue - initialUsd);
    const pnlPct = accountDetails?.pnl_pct
        ?? (initialUsd !== 0 ? (pnl / initialUsd) * 100 : 0);

    const handleRemoveServer = async () => {
        if (!selected || selected.role !== 'admin' || !user?.username || removeLoading) return;
        const confirmed = window.confirm(
            `Remove server "${selected.server_name}"?\n\nThis will stop and remove all related services.`
        );
        if (!confirmed) return;

        setRemoveLoading(true);
        try {
            const data = await removeServer(user.username, {server_name: selected.server_name});
            if (data.auth_error) {
                window.alert(data.auth_error);
                return;
            }
            if (data.error) {
                window.alert(data.error);
                return;
            }
            setAccountDetails(null);
            await refreshServers();
        } catch (err) {
            const msg = err?.data?.auth_error ?? err?.data?.error ?? err.message ?? 'Request failed';
            window.alert(msg);
        } finally {
            setRemoveLoading(false);
        }
    };

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
                ) : detailsLoading ? (
                    <div className="account-empty-state">Loading details…</div>
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
                            {selected.description && (
                                <p className="account-detail-description">{selected.description}</p>
                            )}
                            {selected.role === 'admin' && (
                                <div className="account-detail-actions">
                                    <button
                                        className="account-edit-btn"
                                        onClick={() => setModalServer(selected)}
                                    >
                                        Edit Settings
                                    </button>
                                    <button
                                        className="account-remove-btn"
                                        onClick={handleRemoveServer}
                                        disabled={removeLoading}
                                    >
                                        {removeLoading ? 'Removing…' : 'Remove Server'}
                                    </button>
                                </div>
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
                <ServerModal
                    server={modalServer}
                    username={user?.username ?? ''}
                    onClose={() => setModalServer(undefined)}
                    onSaved={() => {
                        // Refresh server list after create/edit
                        refreshServers();
                    }}
                />
            )}
        </div>
    );
}

export default AccountPage;
