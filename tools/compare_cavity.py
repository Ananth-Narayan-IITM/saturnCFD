#!/usr/bin/env python3
"""Compare a saturnCFD cavity centreline against Ghia, Ghia & Shin (1982).

Usage:
    python3 tools/compare_cavity.py results/cavity_re100/centreline_u.csv --re 100

The simulation writes u at cell centres along x = 0.5, i.e. at
y = (j + 0.5) * h. Ghia's tabulated points do not coincide with those, so the
reference values are evaluated by linear interpolation of the computed profile
at Ghia's y coordinates.

NOTE: the reference values below are transcribed for convenience and should be
checked against the paper before being quoted in a report. Citation:
    U. Ghia, K.N. Ghia, C.T. Shin, J. Comput. Phys. 48 (1982) 387-411, Table I.
"""
import argparse
import sys

GHIA = {
    100: {
        "y": [1.0000, 0.9766, 0.9688, 0.9609, 0.9531, 0.8516, 0.7344, 0.6172,
              0.5000, 0.4531, 0.2813, 0.1719, 0.1016, 0.0703, 0.0625, 0.0547,
              0.0000],
        "u": [1.00000, 0.84123, 0.78871, 0.73722, 0.68717, 0.23151, 0.00332,
              -0.13641, -0.20581, -0.21090, -0.15662, -0.10150, -0.06434,
              -0.04775, -0.04192, -0.03717, 0.00000],
    },
    1000: {
        "y": [1.0000, 0.9766, 0.9688, 0.9609, 0.9531, 0.8516, 0.7344, 0.6172,
              0.5000, 0.4531, 0.2813, 0.1719, 0.1016, 0.0703, 0.0625, 0.0547,
              0.0000],
        "u": [1.00000, 0.65928, 0.57492, 0.51117, 0.46604, 0.33304, 0.18719,
              0.05702, -0.06080, -0.10648, -0.27805, -0.38289, -0.29730,
              -0.22220, -0.20196, -0.18109, 0.00000],
    },
}


def load(path):
    ys, us = [], []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            a, b = line.split(",")[:2]
            ys.append(float(a))
            us.append(float(b))
    if not ys:
        sys.exit(f"no data in {path}")
    return ys, us


def interp(ys, us, y):
    if y <= ys[0]:
        return us[0]
    if y >= ys[-1]:
        return us[-1]
    for i in range(len(ys) - 1):
        if ys[i] <= y <= ys[i + 1]:
            t = (y - ys[i]) / (ys[i + 1] - ys[i])
            return us[i] + t * (us[i + 1] - us[i])
    return float("nan")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv")
    ap.add_argument("--re", type=int, default=100, choices=sorted(GHIA))
    args = ap.parse_args()

    ref = GHIA[args.re]
    ys, us = load(args.csv)

    print(f"{'y':>8} {'Ghia':>10} {'saturnCFD':>11} {'diff':>10}")
    print("-" * 42)
    errs = []
    for y, u in zip(ref["y"], ref["u"]):
        if y in (0.0, 1.0):
            continue
        got = interp(ys, us, y)
        d = got - u
        errs.append(d)
        print(f"{y:8.4f} {u:10.5f} {got:11.5f} {d:+10.5f}")
    print("-" * 42)
    rms = (sum(e * e for e in errs) / len(errs)) ** 0.5
    mx = max(abs(e) for e in errs)
    print(f"RMS deviation = {rms:.5f}   max = {mx:.5f}")


if __name__ == "__main__":
    main()
