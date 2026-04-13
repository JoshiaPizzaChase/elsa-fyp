import React from 'react';

const ROW_HEIGHT_PX = 22; // approximate height per row
const VISIBLE_LEVELS = 10;

const OrderBook = ({bids, asks, lastPrice}) => {
    const formatPrice = (price) => price?.toFixed(2) ?? '-';
    const formatQty = (qty) => qty?.toFixed(4) ?? '-';

    // asks sorted ascending (lowest ask first).
    // Display: lowest ask at bottom (closest to last price), higher asks above it
    // We want to show only the top VISIBLE_LEVELS asks, with best ask at bottom
    const visibleAsks = asks.slice(0, VISIBLE_LEVELS);
    const displayAsks = [...visibleAsks].reverse();

    // bids sorted descending (highest bid first).
    // Display: highest bid at top (closest to last price), lower bids below it
    const visibleBids = bids.slice(0, VISIBLE_LEVELS);

    return (
        <div className="orderbook-container">
            <div className="orderbook-header">
                <span>Price</span>
                <span>Quantity</span>
            </div>

            {/* Asks: best (lowest) ask at bottom, closest to last price */}
            <div
                className="orderbook-asks"
                style={{
                    display: 'flex',
                    flexDirection: 'column',
                    justifyContent: 'flex-end',
                    minHeight: VISIBLE_LEVELS * ROW_HEIGHT_PX,
                    maxHeight: VISIBLE_LEVELS * ROW_HEIGHT_PX,
                    overflow: 'hidden'
                }}
            >
                {displayAsks.map((ask, idx) => (
                    <div key={`ask-${idx}`} className="orderbook-row">
                        <span style={{color: '#ef5350'}}>{formatPrice(ask.price)}</span>
                        <span>{formatQty(ask.quantity)}</span>
                    </div>
                ))}
            </div>

            <div className="last-price">
                {lastPrice ? formatPrice(lastPrice) : '—'}
            </div>

            {/* Bids: best (highest) bid at top, closest to last price */}
            <div
                className="orderbook-bids"
                style={{
                    minHeight: VISIBLE_LEVELS * ROW_HEIGHT_PX,
                    maxHeight: VISIBLE_LEVELS * ROW_HEIGHT_PX,
                    overflow: 'hidden'
                }}
            >
                {visibleBids.map((bid, idx) => (
                    <div key={`bid-${idx}`} className="orderbook-row">
                        <span style={{color: '#26a69a'}}>{formatPrice(bid.price)}</span>
                        <span>{formatQty(bid.quantity)}</span>
                    </div>
                ))}
            </div>
        </div>
    );
};

export default OrderBook;
