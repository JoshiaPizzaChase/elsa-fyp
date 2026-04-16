import pandas as pd
import numpy as np
import json

trades = pd.read_csv('trades.csv')
trades = trades[trades['symbol'] == 'GME'].copy()
trades['price'] = trades['price'] / 100.0
trades['return'] = trades['price'].pct_change()
trades = trades.dropna(subset=['return'])

print("--- Trade Stats ---")
print(f"Total trades for GME: {len(trades)}")
print(f"Mean price: {trades['price'].mean():.2f}")
print(f"Mean return: {trades['return'].mean():.6f}")
print(f"Return std (volatility): {trades['return'].std():.6f}")
print(f"Return skewness: {trades['return'].skew():.4f}")
print(f"Return kurtosis (fat tails check): {trades['return'].kurtosis():.4f}")

orders = pd.read_csv('orders.csv')
orders = orders[orders['symbol'] == 'GME'].copy()

print("\n--- Order Stats ---")
print(f"Total orders for GME: {len(orders)}")
print(f"Buy orders: {len(orders[orders['side'] == 'BID'])}")
print(f"Sell orders: {len(orders[orders['side'] == 'ASK'])}")

spreads = []
with open('mdp.log', 'r') as f:
    for line in f:
        if '{"asks":' in line and '"ticker":"GME"' in line:
            try:
                json_str = line[line.find('{'):]
                data = json.loads(json_str)
                asks = [a for a in data['asks'] if a['price'] > 0 and a['quantity'] > 0]
                bids = [b for b in data['bids'] if b['price'] > 0 and b['quantity'] > 0]
                if asks and bids:
                    best_ask = min([a['price'] for a in asks])
                    best_bid = max([b['price'] for b in bids])
                    spread = best_ask - best_bid
                    if spread > 0:
                        spreads.append(spread / 100.0)
            except Exception:
                pass

spreads = pd.Series(spreads)
print("\n--- Spread Stats ---")
print(f"Average spread: {spreads.mean():.4f}")
print(f"Median spread: {spreads.median():.4f}")
print(f"Spread std: {spreads.std():.4f}")
