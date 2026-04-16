import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import json
import os
import seaborn as sns

sns.set_theme(style="whitegrid")

# Create output directory
os.makedirs('plots', exist_ok=True)

print("Reading trades.csv...")
trades = pd.read_csv('trades.csv')
trades = trades[trades['symbol'] == 'GME'].copy()
trades['timestamp'] = pd.to_datetime(trades['timestamp'])
trades = trades.sort_values('timestamp')

print("Calculating returns...")
# Midprice or trade price for returns? Let's use trade price
trades['price'] = trades['price'] / 100.0 # Assuming price is in cents
trades['return'] = trades['price'].pct_change()
trades = trades.dropna(subset=['return'])

print("Plotting Return Distribution...")
plt.figure(figsize=(10, 6))
sns.histplot(trades['return'], bins=100, kde=True)
plt.title('GME Return Distribution')
plt.xlabel('Return')
plt.ylabel('Frequency')
plt.savefig('plots/gme_return_distribution.png')
plt.close()

print("Plotting Volatility Clustering (Autocorrelation of absolute returns)...")
from statsmodels.graphics.tsaplots import plot_acf
plt.figure(figsize=(10, 6))
plot_acf(trades['return'].abs(), lags=50, alpha=0.05, title='Autocorrelation of Absolute Returns (Volatility Clustering)')
plt.savefig('plots/gme_volatility_clustering.png')
plt.close()

print("Plotting Price Trajectory...")
plt.figure(figsize=(12, 6))
plt.plot(trades['timestamp'], trades['price'], label='Trade Price')
plt.title('GME Price Trajectory')
plt.xlabel('Time')
plt.ylabel('Price')
plt.legend()
plt.savefig('plots/gme_price_trajectory.png')
plt.close()

print("Reading orders.csv...")
orders = pd.read_csv('orders.csv')
orders = orders[orders['symbol'] == 'GME'].copy()
orders['timestamp'] = pd.to_datetime(orders['timestamp'])

print("Parsing mdp.log for Orderbook analysis...")
spreads = []
timestamps = []

with open('mdp.log', 'r') as f:
    for line in f:
        if '{"asks":' in line and '"ticker":"GME"' in line:
            try:
                # Extract the JSON part
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
                        timestamps.append(pd.to_datetime(data['create_timestamp'], unit='ms'))
            except Exception as e:
                continue

if spreads:
    print("Plotting Spread Distribution...")
    plt.figure(figsize=(10, 6))
    sns.histplot(spreads, bins=50, kde=True)
    plt.title('GME Bid-Ask Spread Distribution')
    plt.xlabel('Spread')
    plt.ylabel('Frequency')
    plt.savefig('plots/gme_spread_distribution.png')
    plt.close()
    
    print("Plotting Spread Over Time...")
    plt.figure(figsize=(12, 6))
    plt.plot(timestamps, spreads, alpha=0.6)
    plt.title('GME Bid-Ask Spread Over Time')
    plt.xlabel('Time')
    plt.ylabel('Spread')
    plt.savefig('plots/gme_spread_over_time.png')
    plt.close()

print("Done. Plots saved in report/simulation_results/plots/")
