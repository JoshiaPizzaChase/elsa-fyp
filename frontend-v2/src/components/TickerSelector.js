import React from 'react';

const TICKERS = ['AAPL', 'GOOGL', 'MSFT', 'AMZN', 'TSLA', 'META', 'NVDA', 'JPM'];

const TickerSelector = ({selectedTicker, onTickerChange}) => {
    return (
        <div className="ticker-selector">
            <select
                className="ticker-dropdown"
                value={selectedTicker}
                onChange={(e) => onTickerChange(e.target.value)}
            >
                {TICKERS.map(ticker => (
                    <option key={ticker} value={ticker}>{ticker}</option>
                ))}
            </select>
        </div>
    );
};

export {TICKERS};
export default TickerSelector;

