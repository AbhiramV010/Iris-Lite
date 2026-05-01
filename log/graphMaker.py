import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

file_path = 'log/irisLiteLogs.csv'
df = pd.read_csv(file_path)

df_clean = df.iloc[1:].copy()
df_clean.columns = [
    'Time', 'Empty', 'Zone Monitor', 'Main Tool', 'Compression',
    'Tiered Detection', 'Camera Tool', 'Listener Tool'
] + list(df.columns[8:])

cols_to_plot = ['Time', 'Zone Monitor', 'Main Tool', 'Compression', 'Tiered Detection', 'Camera Tool', 'Listener Tool']
for col in cols_to_plot:
    df_clean[col] = pd.to_numeric(df_clean[col], errors='coerce')

df_plot = df_clean.dropna(subset=['Time']).copy()

if 0.0 not in df_plot['Time'].values:
    zero_row = pd.DataFrame([[0.0] + [0.0]*(len(df_plot.columns)-1)], columns=df_plot.columns)
    df_plot = pd.concat([zero_row, df_plot]).sort_values('Time').reset_index(drop=True)

plt.style.use('default') 
fig, ax = plt.subplots(figsize=(12, 6), facecolor='#FFFFFF')
ax.set_facecolor('#FFFFFF')

styles = {
    'Zone Monitor': {'color': '#1A73E8', 'marker': 'o'},
    'Main Tool': {'color': '#D93025', 'marker': '^'},
    'Compression': {'color': '#795548', 'marker': 's'},
    'Tiered Detection': {'color': '#188038', 'marker': '*'},
    'Camera Tool': {'color': '#E37400', 'marker': 'x'},
    'Listener Tool': {'color': '#00838F', 'marker': 'p'}
}

for label, style in styles.items():
    ax.plot(df_plot['Time'], df_plot[label], label=label,
             color=style['color'], marker=style['marker'], 
             linewidth=1.5, markersize=6)

ax.set_title('Aggregated CPU Usage Of The Iris-Lite Across All Systems', color='black', fontsize=14, pad=20)
ax.set_xlabel('Time (mins)', color='black')
ax.set_ylabel('CPU Usage (% overall)', color='black')

ax.set_xticks(np.arange(0, 10.5, 0.5))
ax.set_yticks([0, 25, 50, 75, 100])
ax.tick_params(colors='black')
ax.set_ylim(0, 100)
ax.set_xlim(0, 10)

ax.grid(color='#E0E0E0', linestyle='-', linewidth=0.5)
ax.legend(facecolor='#FFFFFF', edgecolor='#CCCCCC', labelcolor='black', 
           bbox_to_anchor=(1, 1), loc='upper left')

ax.spines['bottom'].set_color('black')
ax.spines['left'].set_color('black')
ax.spines['top'].set_visible(False)
ax.spines['right'].set_visible(False)

plt.tight_layout()

plt.savefig('log/irisLiteCPUGraph.png', facecolor='#FFFFFF')
plt.show()