import React from 'react';

const RecentTrades = ({trades}) => {
    const formatPrice = (price) => price?.toFixed(2) ?? '-';
    const formatQty = (qty) => qty?.toFixed(4) ?? '-';
    const formatTime = (timestamp) => {
        const date = new Date(timestamp);
        return date.toLocaleTimeString('en-US', {hour12: false});
    };

    return (
        <div className="recent-trades-container">
            <div className="recent-trades-title">Recent Trades</div>
            <div className="recent-trades-header">
                <span>Price</span>
                <span>Qty</span>
                <span>Time</span>
            </div>
            <div className="recent-trades-list">
                {trades.map((trade) => (
                    <div key={trade.trade_id} className="recent-trades-row">
                        <span style={{color: trade.taker_side === 'BUY' ? '#26a69a' : '#ef5350'}}>
                            {formatPrice(trade.price)}
                        </span>
                        <span>{formatQty(trade.quantity)}</span>
                        <span className="recent-trades-time">
                            {formatTime(trade.create_timestamp)}
                        </span>
                    </div>
                ))}
                {trades.length === 0 && (
                    <div className="recent-trades-empty">No trades yet</div>
                )}
            </div>
        </div>
    );
};

export default RecentTrades;

