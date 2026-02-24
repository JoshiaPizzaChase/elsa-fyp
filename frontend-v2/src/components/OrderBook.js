import React, {useRef, useEffect} from 'react';

const ROW_HEIGHT_PX = 22; // approximate height per row
const VISIBLE_LEVELS = 10;

const OrderBook = ({bids, asks, lastPrice}) => {
    const formatPrice = (price) => price?.toFixed(2) ?? '-';
    const formatQty = (qty) => qty?.toFixed(4) ?? '-';

    // asks sorted ascending (lowest ask first).
    // Display highest ask at top, best (lowest) ask just above the mid price.
    const displayAsks = [...asks].reverse();

    const asksRef = useRef(null);

    // Scroll asks to bottom after every render so best (lowest) ask is visible
    useEffect(() => {
        if (asksRef.current) {
            asksRef.current.scrollTop = asksRef.current.scrollHeight;
        }
    });

    return (
        <div className="orderbook-container">
            <div className="orderbook-header">
                <span>Price</span>
                <span>Quantity</span>
            </div>

            {/* Asks: highest price at top, best (lowest) ask at bottom */}
            <div
                ref={asksRef}
                className="orderbook-asks"
                style={{maxHeight: VISIBLE_LEVELS * ROW_HEIGHT_PX}}
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

            {/* Bids: best (highest) bid at top */}
            <div
                className="orderbook-bids"
                style={{maxHeight: VISIBLE_LEVELS * ROW_HEIGHT_PX}}
            >
                {bids.map((bid, idx) => (
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
