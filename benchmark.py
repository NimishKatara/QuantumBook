# benchmark.py
import subprocess
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as mtick
import numpy as np
import os
import sys

CSV    = "results.csv"
BINARY = ".\\quantumbook.exe"

# ── run the C++ binary if csv missing ────────────────────────────────────────

def run_cpp():
    print("results.csv not found — running C++ benchmark first...")
    result = subprocess.run([BINARY], capture_output=False, text=True)
    if result.returncode != 0:
        print("benchmark binary failed — run .\\quantumbook.exe manually first")
        sys.exit(1)

# ── load csv ──────────────────────────────────────────────────────────────────

def load() -> pd.DataFrame:
    df = pd.read_csv(CSV)
    df["orders_K"] = (df["orders_sent"] / 1000).astype(int)
    return df

# ── shared style ──────────────────────────────────────────────────────────────

BLUE   = "#4C72B0"
RED    = "#DD4444"
GREEN  = "#2CA02C"
ORANGE = "#FF7F0E"
GRAY   = "#AAAAAA"

def style_ax(ax, title):
    ax.set_title(title, fontsize=12, fontweight="bold", pad=10)
    ax.spines[["top", "right"]].set_visible(False)
    ax.grid(axis="y", alpha=0.25, linestyle="--")
    ax.tick_params(labelsize=9)

# ── panel 1 : throughput bar ──────────────────────────────────────────────────

def plot_throughput(df, ax):
    bars = ax.bar(df["label"], df["throughput_kops"],
                  color=BLUE, edgecolor="white",
                  linewidth=0.8, width=0.5, zorder=3)

    for bar, val in zip(bars, df["throughput_kops"]):
        ax.text(bar.get_x() + bar.get_width() / 2,
                bar.get_height() + df["throughput_kops"].max() * 0.02,
                f"{val:,.0f}K",
                ha="center", va="bottom", fontsize=9, fontweight="bold")

    ax.set_ylabel("Orders / sec (K)", fontsize=10)
    ax.set_xlabel("Batch size", fontsize=10)
    ax.set_ylim(0, df["throughput_kops"].max() * 1.3)
    ax.yaxis.set_major_formatter(
        mtick.FuncFormatter(lambda x, _: f"{x:,.0f}K"))
    style_ax(ax, "Throughput by batch size")

# ── panel 2 : mean vs p99 latency ────────────────────────────────────────────

def plot_latency(df, ax):
    x     = np.arange(len(df))
    width = 0.32

    b1 = ax.bar(x - width / 2, df["mean_ns"] / 1e6, width,
                label="Mean", color=BLUE, edgecolor="white", zorder=3)
    b2 = ax.bar(x + width / 2, df["p99_ns"]  / 1e6, width,
                label="P99",  color=RED,  edgecolor="white", zorder=3)

    for b, col in [(b1, BLUE), (b2, RED)]:
        for bar in b:
            h = bar.get_height()
            ax.text(bar.get_x() + bar.get_width() / 2,
                    h + df["p99_ns"].max() / 1e6 * 0.02,
                    f"{h:.1f}",
                    ha="center", va="bottom", fontsize=8, color=col)

    ax.set_ylabel("Latency (ms)", fontsize=10)
    ax.set_xlabel("Batch size", fontsize=10)
    ax.set_xticks(x)
    ax.set_xticklabels(df["label"])
    ax.legend(frameon=False, fontsize=9)
    style_ax(ax, "Matching latency  —  mean vs p99")

# ── panel 3 : trades generated vs orders sent ─────────────────────────────────

def plot_trades(df, ax):
    ax.plot(df["label"], df["orders_sent"]  / 1000,
            marker="o", color=GRAY,  linewidth=2,
            markersize=6, label="Orders sent", zorder=3)
    ax.plot(df["label"], df["trades"] / 1000,
            marker="s", color=GREEN, linewidth=2,
            markersize=6, label="Trades generated", zorder=3)

    ax.fill_between(df["label"], df["trades"] / 1000,
                    alpha=0.08, color=GREEN)

    for i, (o, t) in enumerate(zip(df["orders_sent"], df["trades"])):
        pct = t / o * 100
        ax.text(i, t / 1000 + df["orders_sent"].max() / 1000 * 0.03,
                f"{pct:.0f}%",
                ha="center", fontsize=8, color=GREEN, fontweight="bold")

    ax.set_ylabel("Count (K)", fontsize=10)
    ax.set_xlabel("Batch size", fontsize=10)
    ax.yaxis.set_major_formatter(
        mtick.FuncFormatter(lambda x, _: f"{x:.0f}K"))
    ax.legend(frameon=False, fontsize=9)
    style_ax(ax, "Orders sent vs trades generated")

# ── panel 4 : wall time scaling ───────────────────────────────────────────────

def plot_walltime(df, ax):
    ax.plot(df["orders_K"], df["elapsed_ms"],
            marker="D", color=ORANGE, linewidth=2,
            markersize=7, zorder=4, label="Measured")

    # linear fit
    z    = np.polyfit(df["orders_K"], df["elapsed_ms"], 1)
    p    = np.poly1d(z)
    xfit = np.linspace(df["orders_K"].min(), df["orders_K"].max(), 200)
    ax.plot(xfit, p(xfit), "--", color=ORANGE,
            alpha=0.45, linewidth=1.5, label=f"Linear fit  (slope={z[0]:.3f} ms/K)")

    for x, y in zip(df["orders_K"], df["elapsed_ms"]):
        ax.text(x, y + df["elapsed_ms"].max() * 0.03,
                f"{y:.0f}ms",
                ha="center", fontsize=8, color=ORANGE, fontweight="bold")

    ax.set_ylabel("Wall time (ms)", fontsize=10)
    ax.set_xlabel("Orders (K)", fontsize=10)
    ax.legend(frameon=False, fontsize=9)
    style_ax(ax, "Wall time vs batch size  (linear scaling)")

# ── summary table panel ───────────────────────────────────────────────────────

def print_summary(df):
    print("\n" + "="*64)
    print(f"  {'Batch':<12} {'Throughput':>14} {'Mean lat':>12}"
          f" {'P99 lat':>12} {'Trades%':>9}")
    print("="*64)
    for _, row in df.iterrows():
        pct = row["trades"] / row["orders_sent"] * 100
        print(f"  {row['label']:<12}"
              f" {row['throughput_kops']:>11,.0f} K/s"
              f" {row['mean_ns']/1e6:>10.1f} ms"
              f" {row['p99_ns']/1e6:>10.1f} ms"
              f" {pct:>8.1f}%")
    print("="*64 + "\n")

# ── assemble dashboard ────────────────────────────────────────────────────────

def plot_dashboard(df):
    fig, axes = plt.subplots(2, 2, figsize=(13, 8))
    fig.suptitle("QuantumBook  —  Low Latency Order Matching Benchmark",
                 fontsize=14, fontweight="bold", y=1.01)

    plot_throughput(df, axes[0][0])
    plot_latency   (df, axes[0][1])
    plot_trades    (df, axes[1][0])
    plot_walltime  (df, axes[1][1])

    plt.tight_layout()

    out = "quantumbook_benchmark.png"
    plt.savefig(out, dpi=150, bbox_inches="tight")
    print(f"dashboard saved -> {out}")
    plt.show()

# ── entry point ───────────────────────────────────────────────────────────────

if __name__ == "__main__":
    if not os.path.exists(CSV):
        run_cpp()

    df = load()
    print_summary(df)
    plot_dashboard(df)