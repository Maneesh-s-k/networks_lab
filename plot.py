import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
import seaborn as sns
import sys

# --- SCRIPT CONFIGURATION ---
csv_files = ['performance_data_fcfs.csv', 'performance_data_rr.csv']

# --- DATA LOADING AND PREPARATION ---
all_dataframes = []
print("Reading data files...")
try:
    for file in csv_files:
        print(f"- Loading '{file}'...")
        all_dataframes.append(pd.read_csv(file))
    df = pd.concat(all_dataframes, ignore_index=True)
    print("Successfully loaded and combined data.")
except FileNotFoundError as e:
    print(f"\nError: Could not find a file. Make sure your CSV files are in the same directory.")
    print(f"File not found: {e.filename}")
    sys.exit(1)
except Exception as e:
    print(f"\nAn error occurred while reading the CSV files. The file might still be corrupted: {e}")
    sys.exit(1)

# Set a professional plot style for all graphs
sns.set_theme(style="whitegrid")

# --- PLOT 1: TCP THROUGHPUT (1-32 KB) ---
print("\nGenerating Plot 1: TCP Throughput vs. Message Size...")
df_tcp = df[(df['Protocol'] == 'tcp') & (df['MessageSizeKB'].between(1, 32))]
plt.figure(figsize=(12, 8))
ax1 = sns.lineplot(data=df_tcp, x='MessageSizeKB', y='ThroughputKbps', hue='Policy', marker='o', markersize=8, linewidth=2.5)
ax1.set_title('TCP Throughput vs. Message Size', fontsize=18, fontweight='bold')
ax1.set_xlabel('Message Size (KB)', fontsize=14)
ax1.set_ylabel('Throughput (Kbps)', fontsize=14)
ax1.legend(title='Policy', fontsize=11)
ax1.set_xticks(range(0, 33, 2))
ax1.get_yaxis().set_major_formatter(mticker.FuncFormatter(lambda x, p: f'{int(x):,}'))
plt.tight_layout()
plt.savefig('tcp_throughput_vs_size.png')
plt.close() # Close the figure to free memory
print("-> Saved 'tcp_throughput_vs_size.png'")

# --- PLOT 2: UDP THROUGHPUT (1-32 KB) ---
print("Generating Plot 2: UDP Throughput vs. Message Size...")
df_udp = df[(df['Protocol'] == 'udp') & (df['MessageSizeKB'].between(1, 32))]
plt.figure(figsize=(12, 8))
ax2 = sns.lineplot(data=df_udp, x='MessageSizeKB', y='ThroughputKbps', hue='Policy', marker='^', markersize=8, linewidth=2.5)
ax2.set_title('UDP Throughput vs. Message Size', fontsize=18, fontweight='bold')
ax2.set_xlabel('Message Size (KB)', fontsize=14)
ax2.set_ylabel('Throughput (Kbps)', fontsize=14)
ax2.legend(title='Policy', fontsize=11)
ax2.set_xticks(range(0, 33, 2))
ax2.get_yaxis().set_major_formatter(mticker.FuncFormatter(lambda x, p: f'{int(x):,}'))
plt.tight_layout()
plt.savefig('udp_throughput_vs_size.png')
plt.close()
print("-> Saved 'udp_throughput_vs_size.png'")


# --- DATA PREPARATION FOR BULK PLOTS ---
sizes_1mb = [1, 2, 4, 8, 16, 32, 64]
sizes_10mb = [10, 20, 40, 64, 80, 128, 160]
bulk_sizes = sizes_1mb + sizes_10mb

df_exp_b = df[(df['Protocol'] == 'tcp') & (df['MessageSizeKB'].isin(bulk_sizes))]
df_exp_b['ThroughputKbps'] = pd.to_numeric(df_exp_b['ThroughputKbps'], errors='coerce')
avg_throughput = df_exp_b.groupby(['Policy', 'MessageSizeKB'])['ThroughputKbps'].mean().reset_index()
data_1mb = avg_throughput[avg_throughput['MessageSizeKB'].isin(sizes_1mb)]
data_10mb = avg_throughput[avg_throughput['MessageSizeKB'].isin(sizes_10mb)]

# --- PLOT 3: 1MB BULK TRANSFER ---
print("Generating Plot 3: 1MB Bulk Transfer Performance...")
plt.figure(figsize=(10, 7))
ax3 = sns.lineplot(data=data_1mb, x='MessageSizeKB', y='ThroughputKbps', hue='Policy', marker='s', markersize=10, linewidth=3)
ax3.set_title('Average Throughput for 1 MB Bulk Transfer (TCP)', fontsize=16, fontweight='bold')
ax3.set_xlabel('Chunk Size (KB)', fontsize=12)
ax3.set_ylabel('Average Throughput (Kbps)', fontsize=12)
ax3.legend(title='Policy', fontsize=11)
ax3.set_xticks(sizes_1mb)
ax3.get_yaxis().set_major_formatter(mticker.FuncFormatter(lambda x, p: f'{int(x):,}'))
plt.tight_layout()
plt.savefig('bulk_transfer_1mb.png')
plt.close()
print("-> Saved 'bulk_transfer_1mb.png'")

# --- PLOT 4: 10MB BULK TRANSFER ---
print("Generating Plot 4: 10MB Bulk Transfer Performance...")
plt.figure(figsize=(10, 7))
ax4 = sns.lineplot(data=data_10mb, x='MessageSizeKB', y='ThroughputKbps', hue='Policy', marker='D', markersize=9, linewidth=3)
ax4.set_title('Average Throughput for 10 MB Bulk Transfer (TCP)', fontsize=16, fontweight='bold')
ax4.set_xlabel('Chunk Size (KB)', fontsize=12)
ax4.set_ylabel('Average Throughput (Kbps)', fontsize=12)
ax4.legend(title='Policy', fontsize=11)
ax4.set_xticks(sizes_10mb)
ax4.get_yaxis().set_major_formatter(mticker.FuncFormatter(lambda x, p: f'{int(x):,}'))
plt.tight_layout()
plt.savefig('bulk_transfer_10mb.png')
plt.close()
print("-> Saved 'bulk_transfer_10mb.png'")