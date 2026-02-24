import React, {useRef, useEffect, useState, useCallback} from 'react';
import {useParams, useNavigate} from 'react-router-dom';
import {createChart} from 'lightweight-charts';
import useWebSocket from './hooks/useWebSocket';
import OrderBook from './components/OrderBook';
import TradePanel from './components/TradePanel';
import RecentTrades from './components/RecentTrades';
import TickerSelector, {TICKERS} from './components/TickerSelector';
import './App.css';

const TIMEFRAMES = [
    {label: '1m', ms: 60_000},
    {label: '5m', ms: 300_000},
    {label: '15m', ms: 900_000},
    {label: '1h', ms: 3_600_000},
    {label: '4h', ms: 14_400_000},
    {label: '1d', ms: 86_400_000},
];

function App() {
    const {ticker} = useParams();
    const navigate = useNavigate();
    const selectedTicker = TICKERS.includes(ticker?.toUpperCase()) ? ticker.toUpperCase() : 'AAPL';
    const selectedTickerRef = useRef(selectedTicker);

    const handleTickerChange = useCallback((newTicker) => {
        navigate(`/${newTicker}`);
    }, [navigate]);

    const [bids, setBids] = useState([]);
    const [asks, setAsks] = useState([]);
    const [lastTradePrice, setLastTradePrice] = useState(null);
    const [recentTrades, setRecentTrades] = useState([]);
    const seenTradeIdsRef = useRef(new Set());
    const [selectedTimeframe, setSelectedTimeframe] = useState(TIMEFRAMES[0]);

    const chartContainerRef = useRef(null);
    const chartRef = useRef(null);
    const seriesRef = useRef(null);
    const volumeSeriesRef = useRef(null);
    const candlesRef = useRef(new Map());
    const volumeRef = useRef(new Map());
    const allCandlesRef = useRef(new Map());
    const allVolumesRef = useRef(new Map());
    const selectedTimeframeRef = useRef(selectedTimeframe);

    // Keep refs in sync with state
    useEffect(() => {
        selectedTimeframeRef.current = selectedTimeframe;
    }, [selectedTimeframe]);

    useEffect(() => {
        selectedTickerRef.current = selectedTicker;
    }, [selectedTicker]);

    // Reset all data when ticker changes
    useEffect(() => {
        setBids([]);
        setAsks([]);
        setLastTradePrice(null);
        setRecentTrades([]);
        seenTradeIdsRef.current = new Set();
        allCandlesRef.current = new Map();
        allVolumesRef.current = new Map();
        candlesRef.current = new Map();
        volumeRef.current = new Map();
        if (seriesRef.current) {
            seriesRef.current.setData([]);
        }
        if (volumeSeriesRef.current) {
            volumeSeriesRef.current.setData([]);
        }
    }, [selectedTicker]);

    // Switch the visible candles to the selected timeframe
    const rebuildCandles = useCallback((intervalMs) => {
        if (!seriesRef.current) return;
        const tfMap = allCandlesRef.current.get(intervalMs) ?? new Map();
        candlesRef.current = tfMap;
        const sorted = Array.from(tfMap.values()).sort((a, b) => a.time - b.time);
        seriesRef.current.setData(sorted);

        if (volumeSeriesRef.current) {
            const volMap = allVolumesRef.current.get(intervalMs) ?? new Map();
            volumeRef.current = volMap;
            const sortedVol = Array.from(volMap.values()).sort((a, b) => a.time - b.time);
            volumeSeriesRef.current.setData(sortedVol);
        }
    }, []);

    // Handle incoming messages
    const handleMessage = useCallback((data) => {
        // Filter out messages that don't match the selected ticker
        const msgTicker = data.ticker ?? data.symbol;
        if (msgTicker && msgTicker.toUpperCase() !== selectedTickerRef.current) {
            return;
        }

        // Order book snapshot
        if (data.bids && data.asks) {
            const sortedBids = [...data.bids].sort((a, b) => b.price - a.price);
            const sortedAsks = [...data.asks].sort((a, b) => a.price - b.price);
            setBids(sortedBids);
            setAsks(sortedAsks);
        }
        // Trade message
        else if (data.trade_id !== undefined) {
            setLastTradePrice(data.price);
            if (!seenTradeIdsRef.current.has(data.trade_id)) {
                seenTradeIdsRef.current.add(data.trade_id);
                setRecentTrades(prev => [data, ...prev].slice(0, 10));
            }

            // Update candle data for every timeframe so switching is instant
            if (seriesRef.current) {
                const price = data.price;
                const quantity = data.quantity ?? 0;
                const tradeTime = data.create_timestamp;
                const activeIntervalMs = selectedTimeframeRef.current.ms;

                for (const tf of TIMEFRAMES) {
                    const intervalMs = tf.ms;
                    const candleTime = Math.floor(tradeTime / intervalMs) * intervalMs;
                    let tfMap = allCandlesRef.current.get(intervalMs);
                    if (!tfMap) {
                        tfMap = new Map();
                        allCandlesRef.current.set(intervalMs, tfMap);
                    }
                    let candle = tfMap.get(candleTime);
                    if (!candle) {
                        candle = {time: candleTime / 1000, open: price, high: price, low: price, close: price};
                        tfMap.set(candleTime, candle);
                    } else {
                        candle.high = Math.max(candle.high, price);
                        candle.low = Math.min(candle.low, price);
                        candle.close = price;
                    }

                    // Volume
                    let volMap = allVolumesRef.current.get(intervalMs);
                    if (!volMap) {
                        volMap = new Map();
                        allVolumesRef.current.set(intervalMs, volMap);
                    }
                    let vol = volMap.get(candleTime);
                    const volColor = candle.close >= candle.open
                        ? 'rgba(38,166,154,0.5)'
                        : 'rgba(239,83,80,0.5)';
                    if (!vol) {
                        vol = {time: candleTime / 1000, value: quantity, color: volColor};
                        volMap.set(candleTime, vol);
                    } else {
                        vol.value += quantity;
                        vol.color = volColor;
                    }

                    // Live-update the chart only for the active timeframe
                    if (intervalMs === activeIntervalMs) {
                        candlesRef.current = tfMap;
                        seriesRef.current.update(candle);
                        if (volumeSeriesRef.current) {
                            volumeRef.current = volMap;
                            volumeSeriesRef.current.update(vol);
                        }
                    }
                }
            }
        }
    }, []);

    const {isConnected} = useWebSocket('ws://localhost:9000', handleMessage);

    // Initialize chart once
    useEffect(() => {
        if (!chartContainerRef.current) return;

        const chart = createChart(chartContainerRef.current, {
            width: chartContainerRef.current.clientWidth,
            height: 400,
            layout: {
                background: {type: 'solid', color: '#1e1e2f'},
                textColor: '#e0e0e0',
            },
            grid: {
                vertLines: {visible: false},
                horzLines: {visible: false},
            },
            crosshair: {
                mode: 0,
            },
            rightPriceScale: {
                borderColor: '#4a4a5a',
            },
            timeScale: {
                borderColor: '#4a4a5a',
                timeVisible: true,
                secondsVisible: false,
            },
        });

        const series = chart.addCandlestickSeries({
            upColor: '#26a69a',
            downColor: '#ef5350',
            borderDownColor: '#ef5350',
            borderUpColor: '#26a69a',
            wickDownColor: '#ef5350',
            wickUpColor: '#26a69a',
        });

        const volumeSeries = chart.addHistogramSeries({
            priceFormat: {type: 'volume'},
            priceScaleId: '',  // overlay on the same pane
        });
        volumeSeries.priceScale().applyOptions({
            scaleMargins: {
                top: 0.8,   // volume occupies bottom 20%
                bottom: 0,
            },
        });

        chartRef.current = chart;
        seriesRef.current = series;
        volumeSeriesRef.current = volumeSeries;

        const handleResize = () => {
            if (chartContainerRef.current) {
                chart.applyOptions({width: chartContainerRef.current.clientWidth});
            }
        };
        window.addEventListener('resize', handleResize);

        return () => {
            window.removeEventListener('resize', handleResize);
            chart.remove();
        };
    }, []);

    // Rebuild candles when timeframe changes
    useEffect(() => {
        rebuildCandles(selectedTimeframe.ms);
    }, [selectedTimeframe, rebuildCandles]);

    return (
        <div className="app">
            <div className="left-column">
                <div className="chart-container">
                    <div className="chart-toolbar">
                        <TickerSelector
                            selectedTicker={selectedTicker}
                            onTickerChange={handleTickerChange}
                        />
                        <div className="timeframe-selector">
                            {TIMEFRAMES.map(tf => (
                                <button
                                    key={tf.label}
                                    className={`timeframe-btn${selectedTimeframe.label === tf.label ? ' active' : ''}`}
                                    onClick={() => setSelectedTimeframe(tf)}
                                >
                                    {tf.label}
                                </button>
                            ))}
                        </div>
                    </div>
                    <div ref={chartContainerRef} className="chart-wrapper"/>
                </div>
                <TradePanel/>
            </div>

            <div className="right-column">
                <OrderBook
                    bids={bids}
                    asks={asks}
                    lastPrice={lastTradePrice}
                />
                <RecentTrades trades={recentTrades}/>
            </div>

            {!isConnected && (
                <div style={{
                    position: 'fixed',
                    bottom: 16,
                    right: 16,
                    background: '#ef5350',
                    color: 'white',
                    padding: '8px 16px',
                    borderRadius: 4,
                }}>
                    Disconnected from WebSocket
                </div>
            )}
        </div>
    );
}

export default App;