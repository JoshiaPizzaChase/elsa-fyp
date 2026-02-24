import {useEffect, useRef, useState, useCallback} from 'react';

export default function useWebSocket(url, onMessage) {
    const [isConnected, setIsConnected] = useState(false);
    const wsRef = useRef(null);
    // Keep onMessage in a ref so the WebSocket never needs to reconnect when the
    // callback identity changes, and we always call the latest version.
    const onMessageRef = useRef(onMessage);
    useEffect(() => {
        onMessageRef.current = onMessage;
    }, [onMessage]);

    const reconnectTimerRef = useRef(null);
    const intentionalCloseRef = useRef(false);

    const connect = useCallback(() => {
        const ws = new WebSocket(url);
        wsRef.current = ws;

        ws.onopen = () => {
            console.log('WebSocket connected');
            setIsConnected(true);
        };

        ws.onclose = () => {
            console.log('WebSocket disconnected');
            setIsConnected(false);
            if (!intentionalCloseRef.current) {
                reconnectTimerRef.current = setTimeout(connect, 2000);
            }
        };

        ws.onerror = (error) => {
            console.error('WebSocket error:', error);
            ws.close();
        };

        ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                onMessageRef.current(data);
            } catch (err) {
                console.error('Failed to parse message:', err);
            }
        };
    }, [url]); // url-only dep — onMessage changes never cause reconnects

    useEffect(() => {
        intentionalCloseRef.current = false;
        connect();
        return () => {
            intentionalCloseRef.current = true;
            clearTimeout(reconnectTimerRef.current);
            if (wsRef.current) {
                wsRef.current.close();
            }
        };
    }, [connect]);

    return {isConnected};
}
