import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt

df = pd.read_csv('process_stats.csv')

def save_plot(metric, ylabel, filename):
    plt.figure(figsize=(12, 6))
    ax = sns.lineplot(data=df, x='Timestamp', y=metric, hue='Script', marker='o')
    
    for i, label in enumerate(ax.get_xticklabels()):
        if i % 5 != 0:
            label.set_visible(False)
            
    plt.title(f'{ylabel} ($30$s intervals)')
    plt.xticks(rotation=45)
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(filename)

save_plot('CPU_Percent', 'CPU Usage (%)', 'cpu_usage.png')
save_plot('RAM_MB', 'RAM Usage (MB)', 'ram_usage.png')