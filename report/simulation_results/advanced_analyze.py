import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import json
import os
import seaborn as sns
from statsmodels.graphics.tsaplots import plot_acf

sns.set_theme(style="whitegrid")
os.makedirs('plots', exist_ok=True)

print("Reading trades.csv for Return Autocorrelation...")
trades = pd.read_csv('trades.csv')
trades = trades[trades['symbol'] == 'GME'].copy()
trades['price'] = trades['price'] / 100.0
trades['return'] = trades['price'].pct_change()
trades = trades.dropna(subset=['return'])

print("Plotting Autocorrelation of Returns (Bid-Ask Bounce check)...")
plt.figure(figsize=(10, 6))
plot_acf(trades['return'], lags=20, alpha=0.05, title='Autocorrelation of Raw Returns (1st Lag Negative)')
plt.savefig('plots/gme_return_acf.png')
plt.close()

print("Parsing mdp.log for advanced LOB Shape analysis...")
book_shape_data = []

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
                    mid_price = (best_ask + best_bid) / 2.0
                    
                    # Book shape
                    for a in asks:
                        dist = (a['price'] - mid_price) / 100.0
                        if dist <= 0.5: # Focus on top 50 cents
                            book_shape_data.append({'side': 'Ask', 'dist': dist, 'qty': a['quantity']})
                    for b in bids:
                        dist = (mid_price - b['price']) / 100.0
                        if dist <= 0.5:
                            book_shape_data.append({'side': 'Bid', 'dist': dist, 'qty': b['quantity']})
                            
            except Exception:
                continue

if book_shape_data:
    print("Plotting Average Shape of the Book...")
    df_shape = pd.DataFrame(book_shape_data)
    df_shape['dist_rounded'] = df_shape['dist'].round(3) # Round to nearest tenth of a cent to aggregate
    
    shape_agg = df_shape.groupby(['side', 'dist_rounded'])['qty'].mean().reset_index()
    
    plt.figure(figsize=(10, 6))
    ask_shape = shape_agg[shape_agg['side'] == 'Ask'].sort_values('dist_rounded')
    bid_shape = shape_agg[shape_agg['side'] == 'Bid'].sort_values('dist_rounded')
    
    # Plot relative to mid-price (0)
    plt.plot(-bid_shape['dist_rounded'], bid_shape['qty'], color='green', label='Bid Queue', linewidth=2)
    plt.plot(ask_shape['dist_rounded'], ask_shape['qty'], color='red', label='Ask Queue', linewidth=2)
    
    plt.axvline(x=0, color='black', linestyle='--', alpha=0.5, label='Mid-Price')
    plt.title('Average Shape of the Limit Order Book (GME)')
    plt.xlabel('Distance from Mid-Price ($)')
    plt.ylabel('Average Volume')
    plt.legend()
    plt.savefig('plots/gme_lob_shape.png')
    plt.close()

print("Done. Plots saved in report/simulation_results/plots/")
