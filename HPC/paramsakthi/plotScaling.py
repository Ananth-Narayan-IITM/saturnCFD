#!/usr/bin/env python3
"""Plot the Heat scaling results collected in results/scaling.csv."""

from __future__ import annotations

import csv
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

HERE = Path(__file__).resolve().parent
RESULTS = HERE / "results"
FIGURES = RESULTS / "figures"


def load():
    path = RESULTS / "scaling.csv"
    if not path.exists():
        raise SystemExit("run collectResults.py first")
    with open(path, newline="", encoding="utf-8") as fh:
        rows = list(csv.DictReader(fh))
    for r in rows:
        for key in ("mesh", "ranks", "threads", "cells", "steps"):
            r[key] = int(r[key])
        for key in ("dt", "wallSeconds", "secondsPerStep",
                    "cellStepsPerSecond", "l2"):
            r[key] = float(r[key])
    return rows


def select(rows, backend, case, threads=1):
    return sorted(
        (r for r in rows if r["backend"] == backend and r["case"] == case
         and r["threads"] == threads),
        key=lambda r: r["ranks"],
    )


def plot_cpu(rows):
    data = select(rows, "cpu", "heatStrong128")
    if len(data) < 2:
        print("CPU strong data missing; skip cpuStrong")
        return
    base = data[0]["secondsPerStep"]
    ranks = np.array([r["ranks"] for r in data])
    speedup = np.array([base / r["secondsPerStep"] for r in data])
    eff = speedup / (ranks / ranks[0])
    fig, axes = plt.subplots(1, 2, figsize=(10.0, 4.0))
    axes[0].loglog(ranks, speedup, "o-", label="measured")
    axes[0].loglog(ranks, ranks / ranks[0], "--", label="ideal")
    axes[0].set_xlabel("MPI ranks (1 thread/rank)")
    axes[0].set_ylabel("speedup")
    axes[0].set_title("Heat CPU strong scaling, 128^3")
    axes[0].grid(alpha=0.3, which="both")
    axes[0].legend()
    axes[1].semilogx(ranks, 100 * eff, "s-")
    axes[1].set_xlabel("MPI ranks")
    axes[1].set_ylabel("efficiency [%]")
    axes[1].set_ylim(0, 110)
    axes[1].set_title("Parallel efficiency")
    axes[1].grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(FIGURES / "cpuStrong.png", dpi=150)
    fig.savefig(FIGURES / "cpuStrong.pdf")
    plt.close(fig)


def plot_gpu(rows):
    data = select(rows, "gpu", "heatStrong128")
    if len(data) < 2:
        print("GPU strong data missing; skip gpuStrong")
        return
    base = data[0]["secondsPerStep"]
    gpus = np.array([r["ranks"] for r in data])
    speedup = np.array([base / r["secondsPerStep"] for r in data])
    eff = speedup / (gpus / gpus[0])
    fig, axes = plt.subplots(1, 2, figsize=(10.0, 4.0))
    axes[0].plot(gpus, speedup, "o-", label="measured")
    axes[0].plot(gpus, gpus / gpus[0], "--", label="ideal")
    axes[0].set_xlabel("A100 GPUs (one rank/GPU)")
    axes[0].set_ylabel("speedup")
    axes[0].set_title("Heat GPU strong scaling, 128^3")
    axes[0].set_xticks(gpus)
    axes[0].grid(alpha=0.3)
    axes[0].legend()
    axes[1].plot(gpus, 100 * eff, "s-")
    axes[1].set_xlabel("A100 GPUs")
    axes[1].set_ylabel("efficiency [%]")
    axes[1].set_ylim(0, 110)
    axes[1].set_title("GPU efficiency")
    axes[1].set_xticks(gpus)
    axes[1].grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(FIGURES / "gpuStrong.png", dpi=150)
    fig.savefig(FIGURES / "gpuStrong.pdf")
    plt.close(fig)


def plot_compare(rows):
    cpu_one = select(rows, "cpu", "heatStrong128", threads=1)
    cpu_48 = [r for r in cpu_one if r["ranks"] == 48]
    cpu_thr = select(rows, "cpu", "heatStrong128", threads=48)
    gpu = select(rows, "gpu", "heatStrong128")
    labels, times = [], []
    if cpu_one:
        labels.append("1 CPU core")
        times.append(cpu_one[0]["secondsPerStep"])
    if cpu_48:
        labels.append("48 CPU ranks")
        times.append(cpu_48[0]["secondsPerStep"])
    if cpu_thr:
        labels.append("1 rank x 48 threads")
        times.append(cpu_thr[0]["secondsPerStep"])
    for r in gpu:
        labels.append(f"{r['ranks']} A100")
        times.append(r["secondsPerStep"])
    if not labels:
        return
    fig, ax = plt.subplots(figsize=(8.0, 4.4))
    bars = ax.bar(labels, times, color="#4c72b0")
    ax.set_yscale("log")
    ax.set_ylabel("seconds per step")
    ax.set_title("Heat 128^3: CPU versus A100")
    ax.grid(alpha=0.3, axis="y")
    for bar, value in zip(bars, times):
        ax.text(bar.get_x() + bar.get_width() / 2, value,
                f"{value:.3g}", ha="center", va="bottom", fontsize=8)
    plt.setp(ax.get_xticklabels(), rotation=20, ha="right")
    fig.tight_layout()
    fig.savefig(FIGURES / "cpuVsGpu.png", dpi=150)
    fig.savefig(FIGURES / "cpuVsGpu.pdf")
    plt.close(fig)


def plot_accuracy(rows):
    data = [r for r in rows if r["case"] == "heatStrong128"]
    if not data:
        return
    fig, ax = plt.subplots(figsize=(6.4, 4.0))
    for backend, marker in (("cpu", "o"), ("gpu", "s")):
        sel = sorted((r for r in data if r["backend"] == backend),
                     key=lambda r: (r["threads"], r["ranks"]))
        if sel:
            ax.plot([r["ranks"] for r in sel], [r["l2"] for r in sel],
                    marker + "-", label=backend)
    ax.set_xlabel("ranks / GPUs")
    ax.set_ylabel(r"$L^2$ error")
    ax.set_title("Heat 128^3 accuracy")
    ax.grid(alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(FIGURES / "accuracy.png", dpi=150)
    fig.savefig(FIGURES / "accuracy.pdf")
    plt.close(fig)


def main() -> int:
    FIGURES.mkdir(parents=True, exist_ok=True)
    rows = load()
    plot_cpu(rows)
    plot_gpu(rows)
    plot_compare(rows)
    plot_accuracy(rows)
    print(f"figures: {FIGURES}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
