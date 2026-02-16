import React, {useState, useEffect, useRef} from 'react';
import {TrendingUp, TrendingDown, Wifi, WifiOff, Activity} from 'lucide-react';

function OrderBookVisualizer() {
    const [orderBook, setOrderBook] = useState({asks: [], bids: [], ticker: ''});
    const [trades, setTrades] = useState([]);
    const [connectionStatus, setConnectionStatus] = useState('disconnected');
    const [wsUrl, setWsUrl] = useState('ws://localhost:9000');
    const [inputUrl, setInputUrl] = useState('ws://localhost:9000');
    const wsRef = useRef(null);

    const connectWebSocket = () => {
        if (wsRef.current?.readyState === WebSocket.OPEN) {
            wsRef.current.close();
        }

        try {
            const ws = new WebSocket(wsUrl);
            wsRef.current = ws;

            ws.onopen = () => {
                setConnectionStatus('connected');
            };

            ws.onmessage = (event) => {
                try {
                    const data = JSON.parse(event.data);

                    // Handle order book snapshot
                    if (data.asks && data.bids) {
                        const filteredAsks = data.asks.filter(item => item.price > 0 && item.quantity > 0);
                        const filteredBids = data.bids.filter(item => item.price > 0 && item.quantity > 0);

                        setOrderBook({
                            asks: filteredAsks.sort((a, b) => a.price - b.price),
                            bids: filteredBids.sort((a, b) => b.price - a.price),
                            ticker: data.ticker || ''
                        });
                    }
                    // Handle trade message
                    else if (data.trade_id) {
                        setTrades(prev => {
                            const newTrade = {
                                ...data,
                                timestamp: new Date().toLocaleTimeString()
                            };

                            // Keep only latest 50 trades
                            const updatedTrades = [newTrade, ...prev].slice(0, 50);
                            return updatedTrades;
                        });
                    }
                } catch (error) {
                    console.error('Error parsing message:', error);
                }
            };

            ws.onerror = () => {
                setConnectionStatus('error');
            };

            ws.onclose = () => {
                setConnectionStatus('disconnected');
            };
        } catch (error) {
            setConnectionStatus('error');
        }
    };

    const disconnect = () => {
        if (wsRef.current) {
            wsRef.current.close();
            wsRef.current = null;
        }
    };

    useEffect(() => {
        return () => {
            if (wsRef.current) {
                wsRef.current.close();
            }
        };
    }, []);

    const getMaxQuantity = () => {
        const allQuantities = [
            ...orderBook.asks.map(a => a.quantity),
            ...orderBook.bids.map(b => b.quantity)
        ];
        return Math.max(...allQuantities, 1);
    };

    const maxQuantity = getMaxQuantity();
    const spread = orderBook.asks.length > 0 && orderBook.bids.length > 0
        ? (orderBook.asks[0].price - orderBook.bids[0].price).toFixed(2)
        : '0.00';

    return (
        <div className="min-h-screen bg-gray-900 text-white p-4">
            <div className="max-w-7xl mx-auto">
                <div className="bg-gray-800 rounded-lg shadow-xl p-6 mb-6">
                    <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-4 mb-4">
                        <div>
                            <h1 className="text-3xl font-bold mb-1">Order Book</h1>
                            {orderBook.ticker && (
                                <p className="text-gray-400 text-lg">{orderBook.ticker}</p>
                            )}
                        </div>
                        <div className="flex items-center gap-2">
                            {connectionStatus === 'connected' ? (
                                <div className="flex items-center gap-2 text-green-400">
                                    <Wifi className="w-5 h-5"/>
                                    <span className="text-sm">Connected</span>
                                </div>
                            ) : (
                                <div className="flex items-center gap-2 text-red-400">
                                    <WifiOff className="w-5 h-5"/>
                                    <span className="text-sm">Disconnected</span>
                                </div>
                            )}
                        </div>
                    </div>

                    <div className="flex flex-col sm:flex-row gap-2">
                        <input
                            type="text"
                            value={inputUrl}
                            onChange={(e) => setInputUrl(e.target.value)}
                            placeholder="WebSocket URL"
                            className="flex-1 px-4 py-2 bg-gray-700 rounded border border-gray-600 focus:outline-none focus:border-blue-500"
                        />
                        {connectionStatus === 'connected' ? (
                            <button
                                onClick={disconnect}
                                className="px-6 py-2 bg-red-600 hover:bg-red-700 rounded font-medium transition-colors"
                            >
                                Disconnect
                            </button>
                        ) : (
                            <button
                                onClick={() => {
                                    setWsUrl(inputUrl);
                                    setTimeout(connectWebSocket, 0);
                                }}
                                className="px-6 py-2 bg-blue-600 hover:bg-blue-700 rounded font-medium transition-colors"
                            >
                                Connect
                            </button>
                        )}
                    </div>
                </div>

                <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
                    {/* Order Book Section - 2 columns on larger screens */}
                    <div className="lg:col-span-2 grid grid-cols-1 lg:grid-cols-2 gap-6">
                        <div className="bg-gray-800 rounded-lg shadow-xl overflow-hidden">
                            <div className="bg-red-900 bg-opacity-30 px-6 py-3 border-b border-gray-700">
                                <div className="flex items-center justify-between">
                                    <h2 className="text-xl font-bold flex items-center gap-2">
                                        <TrendingDown className="w-5 h-5 text-red-400"/>
                                        Asks
                                    </h2>
                                    <span className="text-sm text-gray-400">
                      {orderBook.asks.length} orders
                    </span>
                                </div>
                            </div>
                            <div className="max-h-96 overflow-y-auto">
                                <table className="w-full">
                                    <thead className="bg-gray-700 sticky top-0">
                                    <tr>
                                        <th className="px-4 py-2 text-left text-sm font-semibold">Price</th>
                                        <th className="px-4 py-2 text-right text-sm font-semibold">Quantity</th>
                                        <th className="px-4 py-2 text-right text-sm font-semibold">Total</th>
                                    </tr>
                                    </thead>
                                    <tbody>
                                    {orderBook.asks.map((ask, index) => {
                                        const widthPercent = (ask.quantity / maxQuantity) * 100;
                                        return (
                                            <tr key={index}
                                                className="border-b border-gray-700 hover:bg-gray-750 relative">
                                                <td className="px-4 py-2 text-red-400 font-mono relative z-10">
                                                    {ask.price.toFixed(2)}
                                                </td>
                                                <td className="px-4 py-2 text-right font-mono relative z-10">
                                                    {ask.quantity.toLocaleString()}
                                                </td>
                                                <td className="px-4 py-2 text-right font-mono text-gray-400 relative z-10">
                                                    {(ask.price * ask.quantity).toFixed(2)}
                                                </td>
                                                <td className="absolute inset-0 pointer-events-none">
                                                    <div
                                                        className="h-full bg-red-900 bg-opacity-20"
                                                        style={{width: `${widthPercent}%`}}
                                                    />
                                                </td>
                                            </tr>
                                        );
                                    })}
                                    {orderBook.asks.length === 0 && (
                                        <tr>
                                            <td colSpan="3" className="px-4 py-8 text-center text-gray-500">
                                                No ask orders
                                            </td>
                                        </tr>
                                    )}
                                    </tbody>
                                </table>
                            </div>
                        </div>

                        <div className="bg-gray-800 rounded-lg shadow-xl overflow-hidden">
                            <div className="bg-green-900 bg-opacity-30 px-6 py-3 border-b border-gray-700">
                                <div className="flex items-center justify-between">
                                    <h2 className="text-xl font-bold flex items-center gap-2">
                                        <TrendingUp className="w-5 h-5 text-green-400"/>
                                        Bids
                                    </h2>
                                    <span className="text-sm text-gray-400">
                      {orderBook.bids.length} orders
                    </span>
                                </div>
                            </div>
                            <div className="max-h-96 overflow-y-auto">
                                <table className="w-full">
                                    <thead className="bg-gray-700 sticky top-0">
                                    <tr>
                                        <th className="px-4 py-2 text-left text-sm font-semibold">Price</th>
                                        <th className="px-4 py-2 text-right text-sm font-semibold">Quantity</th>
                                        <th className="px-4 py-2 text-right text-sm font-semibold">Total</th>
                                    </tr>
                                    </thead>
                                    <tbody>
                                    {orderBook.bids.map((bid, index) => {
                                        const widthPercent = (bid.quantity / maxQuantity) * 100;
                                        return (
                                            <tr key={index}
                                                className="border-b border-gray-700 hover:bg-gray-750 relative">
                                                <td className="px-4 py-2 text-green-400 font-mono relative z-10">
                                                    {bid.price.toFixed(2)}
                                                </td>
                                                <td className="px-4 py-2 text-right font-mono relative z-10">
                                                    {bid.quantity.toLocaleString()}
                                                </td>
                                                <td className="px-4 py-2 text-right font-mono text-gray-400 relative z-10">
                                                    {(bid.price * bid.quantity).toFixed(2)}
                                                </td>
                                                <td className="absolute inset-0 pointer-events-none">
                                                    <div
                                                        className="h-full bg-green-900 bg-opacity-20"
                                                        style={{width: `${widthPercent}%`}}
                                                    />
                                                </td>
                                            </tr>
                                        );
                                    })}
                                    {orderBook.bids.length === 0 && (
                                        <tr>
                                            <td colSpan="3" className="px-4 py-8 text-center text-gray-500">
                                                No bid orders
                                            </td>
                                        </tr>
                                    )}
                                    </tbody>
                                </table>
                            </div>
                        </div>
                    </div>

                    {/* Trades Panel - 1 column on larger screens */}
                    <div className="bg-gray-800 rounded-lg shadow-xl overflow-hidden">
                        <div className="bg-blue-900 bg-opacity-30 px-6 py-3 border-b border-gray-700">
                            <div className="flex items-center justify-between">
                                <h2 className="text-xl font-bold flex items-center gap-2">
                                    <Activity className="w-5 h-5 text-blue-400"/>
                                    Recent Trades
                                </h2>
                                <span className="text-sm text-gray-400">
                    {trades.length} trades
                  </span>
                            </div>
                        </div>
                        <div className="max-h-[calc(100vh-300px)] overflow-y-auto">
                            <table className="w-full">
                                <thead className="bg-gray-700 sticky top-0">
                                <tr>
                                    <th className="px-4 py-2 text-left text-xs font-semibold text-gray-300">Time</th>
                                    <th className="px-4 py-2 text-left text-xs font-semibold text-gray-300">Price</th>
                                    <th className="px-4 py-2 text-right text-xs font-semibold text-gray-300">Qty</th>
                                    <th className="px-4 py-2 text-left text-xs font-semibold text-gray-300">Side</th>
                                </tr>
                                </thead>
                                <tbody>
                                {trades.map((trade, index) => (
                                    <tr
                                        key={trade.trade_id}
                                        className={`border-b border-gray-700 hover:bg-gray-750 ${
                                            index === 0 ? 'bg-gray-750' : ''
                                        }`}
                                    >
                                        <td className="px-4 py-2 text-xs text-gray-400 font-mono">
                                            {trade.timestamp}
                                        </td>
                                        <td className={`px-4 py-2 font-mono text-sm ${
                                            trade.taker_side === 'BUY' ? 'text-green-400' : 'text-red-400'
                                        }`}>
                                            {trade.price.toFixed(2)}
                                        </td>
                                        <td className="px-4 py-2 text-right font-mono text-sm">
                                            {trade.quantity.toLocaleString()}
                                        </td>
                                        <td className="px-4 py-2">
                          <span className={`inline-flex items-center px-2 py-1 rounded text-xs font-medium ${
                              trade.taker_side === 'BUY'
                                  ? 'bg-green-900 bg-opacity-30 text-green-300'
                                  : 'bg-red-900 bg-opacity-30 text-red-300'
                          }`}>
                            {trade.taker_side}
                          </span>
                                        </td>
                                    </tr>
                                ))}
                                {trades.length === 0 && (
                                    <tr>
                                        <td colSpan="4" className="px-4 py-8 text-center text-gray-500">
                                            No trades yet
                                        </td>
                                    </tr>
                                )}
                                </tbody>
                            </table>
                        </div>
                        <div className="px-4 py-2 border-t border-gray-700 bg-gray-750">
                            <p className="text-xs text-gray-400 text-center">
                                Showing last {Math.min(trades.length, 50)} trades
                            </p>
                        </div>
                    </div>
                </div>

                {orderBook.asks.length > 0 && orderBook.bids.length > 0 && (
                    <div className="mt-6 bg-gray-800 rounded-lg shadow-xl p-6">
                        <div className="grid grid-cols-1 sm:grid-cols-3 gap-4 text-center">
                            <div>
                                <p className="text-gray-400 text-sm mb-1">Best Ask</p>
                                <p className="text-red-400 text-2xl font-bold font-mono">
                                    {orderBook.asks[0].price.toFixed(2)}
                                </p>
                            </div>
                            <div>
                                <p className="text-gray-400 text-sm mb-1">Spread</p>
                                <p className="text-yellow-400 text-2xl font-bold font-mono">
                                    {spread}
                                </p>
                            </div>
                            <div>
                                <p className="text-gray-400 text-sm mb-1">Best Bid</p>
                                <p className="text-green-400 text-2xl font-bold font-mono">
                                    {orderBook.bids[0].price.toFixed(2)}
                                </p>
                            </div>
                        </div>
                    </div>
                )}
            </div>
        </div>
    );
}

export default OrderBookVisualizer;