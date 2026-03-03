import React from 'react';

const DEFAULT_TICKERS = ['AAPL', 'GOOGL', 'MSFT', 'AMZN', 'TSLA', 'META', 'NVDA', 'JPM'];

const TickerSelector = ({selectedTicker, onTickerChange, tickers}) => {
    const list = tickers && tickers.length > 0 ? tickers : DEFAULT_TICKERS;
    return (
        <div className="ticker-selector">
            <select
                className="ticker-dropdown"
                value={selectedTicker}
                onChange={(e) => onTickerChange(e.target.value)}
            >
                {list.map(ticker => (
                    <option key={ticker} value={ticker}>{ticker}</option>
                ))}
            </select>
        </div>
    );
};

export {DEFAULT_TICKERS};
export default TickerSelector;

