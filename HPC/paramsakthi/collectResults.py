#!/usr/bin/env python3
"""Collect the Heat scaling JSON files into one CSV."""

from __future__ import annotations

import csv
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
RESULTS = HERE / "results"


def main() -> int:
    rows = []
    for path in sorted(RESULTS.rglob("*.json")):
        try:
            meta = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue
        exact = meta.get("exact")
        if not isinstance(exact, dict):
            continue
        backend = "gpu" if "GPU" in str(meta.get("backend", "")) else "cpu"
        stem = path.stem
        case = stem.split("-")[0]
        threads = 48 if "-threads-" in stem else 1
        samples = int(exact.get("samples", 0))
        steps = int(meta.get("steps", 0))
        wall = float(meta.get("wallSeconds", 0.0))
        rows.append({
            "backend": backend,
            "case": case,
            "mesh": round(samples ** (1.0 / 3.0)) if samples else 0,
            "ranks": int(meta.get("ranks", 1)),
            "threads": threads,
            "cells": samples,
            "steps": steps,
            "dt": float(meta.get("dt", 0.0)),
            "wallSeconds": wall,
            "secondsPerStep": wall / steps if steps else 0.0,
            "cellStepsPerSecond": (samples * steps / wall) if wall else 0.0,
            "l2": float(exact.get("l2", 0.0)),
        })

    if not rows:
        raise SystemExit("no Heat JSON results found under results/")
    rows.sort(key=lambda r: (r["backend"], r["case"], r["ranks"],
                             r["threads"]))
    out = RESULTS / "scaling.csv"
    with open(out, "w", newline="", encoding="utf-8") as fh:
        writer = csv.DictWriter(fh, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    print(f"{'backend':7s} {'case':14s} {'mesh':>4s} {'ranks':>5s} "
          f"{'thr':>3s} {'steps':>5s} {'wall[s]':>10s} {'s/step':>10s} "
          f"{'Gcellstep/s':>12s} {'L2':>10s}")
    for r in rows:
        print(f"{r['backend']:7s} {r['case']:14s} {r['mesh']:4d} "
              f"{r['ranks']:5d} {r['threads']:3d} {r['steps']:5d} "
              f"{r['wallSeconds']:10.3f} {r['secondsPerStep']:10.5f} "
              f"{r['cellStepsPerSecond'] / 1e9:12.3f} {r['l2']:10.4e}")
    print(f"\nwrote {out}  ({len(rows)} runs)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
