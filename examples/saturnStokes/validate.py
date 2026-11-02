#!/usr/bin/env python3
"""Validate the saturnStokes 2D/3D examples in one place.

For each dimension and each case (Poiseuille, Couette, wall MMS) this script:

- runs the direct LU and fieldsplit solvers on the same mesh;
- records wall time, peak process-tree RSS, iterations and analytic error;
- checks that fieldsplit and direct agree;
- runs the wall-MMS convergence study;
- writes one organized results tree and a set of validation/comparison plots.

Usage, from examples/saturnStokes:

    python3 validate.py --dim 2d --cli saturnCPU
    python3 validate.py --dim 3d --cli saturnCPU
    python3 validate.py --dim both --cli saturnCPU

Use --quick for the case meshes instead of the larger solver-comparison meshes.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import shutil
import subprocess
import time
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.cm import ScalarMappable
from matplotlib.colors import Normalize

ROOT = Path(__file__).resolve().parent
DIMS = {
    "2d": {
        "folder": "saturnStokes2D",
        "dim": 2,
        "compare": (128, 128, 1),
        "quick": (64, 64, 1),
        "mms": (16, 32, 64),
        "mms_quick": (16, 32),
    },
    "3d": {
        "folder": "saturnStokes3D",
        "dim": 3,
        "compare": (24, 24, 24),
        "quick": (24, 24, 8),
        "mms": (8, 12, 16),
        "mms_quick": (8, 12),
    },
}
CASES = (("poiseuille", "poiseuille.yaml"),
         ("couette", "couette.yaml"),
         ("mmsWall", "mmsWall.yaml"))
COLORS = {"direct": "#4c72b0", "fieldsplit": "#55a868"}


def read_simple_yaml(path: Path) -> dict:
    sections: dict[str, dict[str, str]] = {}
    top: dict[str, str] = {}
    section: str | None = None
    for raw in path.read_text().splitlines():
        line = raw.split("#", 1)[0].rstrip()
        if not line.strip():
            continue
        if not line.startswith((" ", "\t")) and line.rstrip().endswith(":"):
            section = line.strip()[:-1]
            sections.setdefault(section, {})
            continue
        if ":" not in line:
            continue
        key, value = line.split(":", 1)
        key = key.strip()
        value = value.strip().strip('"').strip("'")
        if not value:
            continue
        if section is None:
            top[key] = value
        else:
            sections[section][key] = value
    return {**top, **sections}


def value(values: dict, section: str, key: str, default: str) -> str:
    try:
        return values[section][key]
    except KeyError:
        return default


def emit_case(path: Path, base: dict, mesh, solver: str, prefix: Path,
              ranks: int) -> None:
    nx, ny, nz = mesh
    box = base.get("box", {})
    st = base.get("stokes", {})
    case = st.get("case", "poiseuille")
    mu = st.get("mu", "1.0")
    lines = [
        "solver: saturnStokes",
        "",
        "units:",
        "  system: SI",
        "",
        "mesh:",
        "  type: box",
        f"  nx: {nx}",
        f"  ny: {ny}",
        f"  nz: {nz}",
        "  box:",
        f"    xmin: {box.get('xmin', '0.0')}",
        f"    xmax: {box.get('xmax', '1.0')}",
        f"    ymin: {box.get('ymin', '0.0')}",
        f"    ymax: {box.get('ymax', '1.0')}",
        f"    zmin: {box.get('zmin', '0.0')}",
        f"    zmax: {box.get('zmax', '1.0')}",
        "",
        "stokes:",
        f"  case: {case}",
        f"  mu: {mu}",
    ]
    if case == "poiseuille":
        lines.append(f"  forceX: {st.get('forceX', '1.0')}")
    elif case == "couette":
        lines.append(f"  wallVelocityX: {st.get('wallVelocityX', '1.0')}")
    elif case == "mms":
        lines.append("  mmsWall: true")
    lines.append(f"  linearSolver: {solver}")
    if solver == "fieldsplit":
        vpc = "ilu" if ranks == 1 else "gamg"
        ppc = "jacobi" if ranks == 1 else "gamg"
        lines += [
            "  velocityKsp: gmres",
            f"  velocityPc: {vpc}",
            "  pressureKsp: preonly",
            f"  pressurePc: {ppc}",
        ]
    lines += [
        "",
        "output:",
        f"  prefix: {prefix}",
        "  csv: true",
        "  json: true",
        "  exact: true",
        "  vtkEvery: 0",
        "",
    ]
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines), encoding="utf-8")


def tree_rss(pid: int) -> int:
    total = 0
    stack = [pid]
    seen: set[int] = set()
    while stack:
        p = stack.pop()
        if p in seen:
            continue
        seen.add(p)
        try:
            with open(f"/proc/{p}/status", encoding="utf-8") as fh:
                for line in fh:
                    if line.startswith("VmRSS:"):
                        total += int(line.split()[1]) * 1024
                        break
        except (FileNotFoundError, ProcessLookupError):
            continue
        try:
            with open(f"/proc/{p}/task/{p}/children", encoding="utf-8") as fh:
                stack.extend(int(x) for x in fh.read().split())
        except (FileNotFoundError, ProcessLookupError):
            pass
    return total


def run_case(command: list[str], log: Path) -> tuple[int, float, int]:
    t0 = time.perf_counter()
    with open(log, "w", encoding="utf-8") as out:
        env = os.environ.copy()
        env["OMP_NUM_THREADS"] = "1"
        proc = subprocess.Popen(command, cwd=ROOT, stdout=out,
                                stderr=subprocess.STDOUT, env=env)
        peak = tree_rss(proc.pid)
        while proc.poll() is None:
            peak = max(peak, tree_rss(proc.pid))
            time.sleep(0.05)
        proc.wait()
    return proc.returncode, time.perf_counter() - t0, peak


def load(prefix: Path):
    with open(prefix.with_suffix(".json"), encoding="utf-8") as fh:
        meta = json.load(fh)
    data = np.genfromtxt(prefix.with_suffix(".csv"), delimiter=",", names=True)
    return meta, np.atleast_1d(data)


def to_grid(x, y, values):
    xu = np.unique(x)
    yu = np.unique(y)
    grid = np.full((xu.size, yu.size), np.nan)
    grid[np.searchsorted(xu, x), np.searchsorted(yu, y)] = values
    return xu, yu, grid


def midpoint(values, target=0.5):
    unique = np.unique(values)
    return unique[np.argmin(np.abs(unique - target))]

def analytic_profile(base: dict, y, case: str):
    st = base.get("stokes", {})
    box = base.get("box", {})
    mu = float(st.get("mu", "1.0"))
    y0 = float(box.get("ymin", "0.0"))
    ymax = float(box.get("ymax", "1.0"))
    height = ymax - y0
    yy = y - y0
    if case == "poiseuille":
        g = float(st.get("forceX", "1.0"))
        return g / (2.0 * mu) * yy * (height - yy)
    u = float(st.get("wallVelocityX", "1.0"))
    return u * yy / height


def exact_consistency(base: dict, data, case: str) -> float:
    if case not in ("poiseuille", "couette"):
        return 0.0
    return float(np.max(np.abs(np.asarray(data["u_exact"], dtype=float)
                               - analytic_profile(base, np.asarray(data["y"]),
                                                  case))))


def plot_2d_validation(meta, data, case, out):
    x = data["x"]
    y = data["y"]
    u = data["u"]
    ue = data["u_exact"]
    err = u - ue
    zmid = midpoint(data["z"])
    plane = np.isclose(data["z"], zmid)
    xu, yu, num = to_grid(x[plane], y[plane], u[plane])
    _, _, exact = to_grid(x[plane], y[plane], ue[plane])
    _, _, error = to_grid(x[plane], y[plane], err[plane])
    yu_all = np.unique(y)
    num_profile = np.array([np.mean(u[np.isclose(y, yy)]) for yy in yu_all])
    exact_profile = np.array(
        [np.mean(ue[np.isclose(y, yy)]) for yy in yu_all])

    fig, axes = plt.subplots(2, 2, figsize=(11.0, 8.0))
    panels = [(axes[0, 0], num.T, "saturnStokes"),
              (axes[0, 1], exact.T, "analytic"),
              (axes[1, 0], error.T, "error")]
    for ax, grid, title in panels:
        if title == "error":
            vmax = float(np.nanmax(np.abs(grid)))
            vmax = vmax if vmax > 0 else 1e-30
            im = ax.pcolormesh(xu, yu, grid, shading="auto", cmap="RdBu_r",
                               vmin=-vmax, vmax=vmax)
        else:
            im = ax.pcolormesh(xu, yu, grid, shading="auto", cmap="viridis")
        ax.set_aspect("equal")
        ax.set_xlabel("x [m]")
        ax.set_ylabel("y [m]")
        ax.set_title(f"{title} (z={zmid:g})")
        fig.colorbar(im, ax=ax, shrink=0.82)

    ax = axes[1, 1]
    ax.plot(exact_profile, yu_all, color="black", linewidth=2.0,
            label="analytic")
    ax.plot(num_profile, yu_all, "o", color="#1f77b4", markersize=2.5,
            label="saturnStokes")
    ax.set_xlabel("u [m/s]")
    ax.set_ylabel("y [m]")
    ax.set_title("Profile (averaged over periodic direction)")
    ax.legend(loc="best")
    ax.grid(alpha=0.3)
    fig.suptitle(f"saturnStokes {case}  L2={meta['exact']['l2']:.3e}")
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    fig.savefig(out.with_suffix(".png"), dpi=150)
    fig.savefig(out.with_suffix(".pdf"))
    plt.close(fig)


def surface_grid(data, values, axis, target):
    plane = np.isclose(data[axis], target)
    if axis == "z":
        a, b = data["x"][plane], data["y"][plane]
    elif axis == "x":
        a, b = data["z"][plane], data["y"][plane]
    else:
        a, b = data["x"][plane], data["z"][plane]
    au, bu, grid = to_grid(a, b, values[plane])
    A, B = np.meshgrid(au, bu, indexing="ij")
    return au, bu, A, B, grid


def draw_surface(ax, data, values, cmap, norm, title, zmid, zlabel, zlim=None):
    xu, yu, X, Y, Z = surface_grid(data, values, "z", zmid)
    ax.plot_surface(X, Y, Z, cmap=cmap, norm=norm, rstride=1, cstride=1,
                    linewidth=0.15, edgecolor=(0, 0, 0, 0.12),
                    antialiased=True, shade=False)
    ax.set_xlabel("x [m]")
    ax.set_ylabel("y [m]")
    ax.set_zlabel(zlabel)
    ax.set_title(title)
    ax.view_init(elev=26, azim=-62)
    ax.set_box_aspect((1.0, 1.0, 0.75))
    ax.set_xlim(float(xu[0]), float(xu[-1]))
    ax.set_ylim(float(yu[0]), float(yu[-1]))
    if zlim is not None:
        ax.set_zlim(*zlim)
    ax.grid(False)


def plot_3d_validation(meta, data, case, out):
    zmid = midpoint(data["z"])
    u = data["u"]
    ue = data["u_exact"]
    err = u - ue
    vmin = float(min(np.min(u), np.min(ue)))
    vmax = float(max(np.max(u), np.max(ue)))
    if vmax - vmin < 1e-14:
        vmax = vmin + 1e-14
    norm = Normalize(vmin=vmin, vmax=vmax)
    emax = float(np.max(np.abs(err)))
    emax = emax if emax > 1e-30 else 1e-30
    enorm = Normalize(vmin=-emax, vmax=emax)

    fig = plt.figure(figsize=(15.5, 5.6))
    ax_num = fig.add_subplot(1, 3, 1, projection="3d")
    ax_exact = fig.add_subplot(1, 3, 2, projection="3d")
    ax_error = fig.add_subplot(1, 3, 3, projection="3d")
    draw_surface(ax_num, data, u, "viridis", norm, "saturnStokes", zmid,
                 "u [m/s]")
    draw_surface(ax_exact, data, ue, "viridis", norm, "analytic", zmid,
                 "u [m/s]")
    draw_surface(ax_error, data, err, "RdBu_r", enorm,
                 "error (numerical - analytic)", zmid, "error [m/s]",
                 (-1.05 * emax, 1.05 * emax))
    sm = ScalarMappable(norm=norm, cmap="viridis")
    sm.set_array([])
    fig.colorbar(sm, ax=[ax_num, ax_exact], orientation="horizontal",
                 fraction=0.04, pad=0.02, shrink=0.7, label="u [m/s]")
    sme = ScalarMappable(norm=enorm, cmap="RdBu_r")
    sme.set_array([])
    fig.colorbar(sme, ax=ax_error, orientation="horizontal", fraction=0.04,
                 pad=0.02, shrink=0.7,
                 label="u numerical - analytic [m/s]")
    fig.suptitle(
        f"saturnStokes 3D {case} velocity profile at z={zmid:g} m",
        fontsize=13)
    fig.subplots_adjust(left=0.02, right=0.98, top=0.88, bottom=0.12,
                        wspace=0.02)
    fig.savefig(out.with_suffix(".png"), dpi=150)
    fig.savefig(out.with_suffix(".pdf"))
    plt.close(fig)


def solver_diff(a, b):
    if a.size != b.size:
        raise RuntimeError("direct and fieldsplit CSV sizes differ")
    if not (np.allclose(a["x"], b["x"]) and np.allclose(a["y"], b["y"])
            and np.allclose(a["z"], b["z"])):
        raise RuntimeError("direct and fieldsplit grids differ")
    du = a["u"] - b["u"]
    dv = a["v"] - b["v"]
    dw = a["w"] - b["w"]
    dp = a["p"] - b["p"]
    dp -= np.mean(dp)
    vel = np.sqrt(du * du + dv * dv + dw * dw)
    return float(np.sqrt(np.mean(vel * vel))), float(np.sqrt(np.mean(dp * dp)))


def plot_solver_bars(rows: list[dict], title: str, out: Path) -> None:
    direct = next(r for r in rows if r["solver"] == "direct")
    fs = next(r for r in rows if r["solver"] == "fieldsplit")
    fig, axes = plt.subplots(1, 3, figsize=(11.5, 4.2))
    axes[0].bar(["direct", "fieldsplit"], [direct["wall"], fs["wall"]],
                color=[COLORS["direct"], COLORS["fieldsplit"]])
    axes[0].set_ylabel("wall time [s]")
    axes[0].set_title("Time")
    for i, r in enumerate((direct, fs)):
        axes[0].text(i, r["wall"], f"{r['wall']:.2f}", ha="center",
                     va="bottom", fontsize=9)
    dm = direct["peak"] / 2**20
    fm = fs["peak"] / 2**20
    axes[1].bar(["direct", "fieldsplit"], [dm, fm],
                color=[COLORS["direct"], COLORS["fieldsplit"]])
    axes[1].set_ylabel("peak RSS [MB]")
    axes[1].set_title("Memory (process tree)")
    for i, v in enumerate((dm, fm)):
        axes[1].text(i, v, f"{v:.0f}", ha="center", va="bottom", fontsize=9)
    axes[2].bar(["velocity", "pressure"], [fs["velDiff"], fs["pDiff"]],
                color=["#c44e52", "#8172b3"])
    axes[2].set_yscale("log")
    axes[2].set_ylabel("fieldsplit - direct (rms)")
    axes[2].set_title("Solution difference")
    for i, v in enumerate((fs["velDiff"], fs["pDiff"])):
        axes[2].text(i, max(v, 1e-30), f"{v:.2e}", ha="center", va="bottom",
                     fontsize=8)
    for ax in axes:
        ax.grid(alpha=0.3, axis="y")
    fig.suptitle(title)
    fig.tight_layout(rect=(0, 0, 1, 0.93))
    fig.savefig(out.with_suffix(".png"), dpi=150)
    fig.savefig(out.with_suffix(".pdf"))
    plt.close(fig)


def plot_mms_order(series: list[dict], out: Path) -> tuple[float, float]:
    hs = np.array([s["h"] for s in series])
    ue = np.array([s["l2"] for s in series])
    pe = np.array([s["pL2"] for s in series])
    order_u = math.log(ue[-2] / ue[-1]) / math.log(hs[-2] / hs[-1])
    order_p = math.log(pe[-2] / pe[-1]) / math.log(hs[-2] / hs[-1])
    fig, ax = plt.subplots(figsize=(6.4, 4.4))
    ax.loglog(hs, ue, "o-", label=f"velocity (order {order_u:.2f})")
    ax.loglog(hs, pe, "s-", label=f"pressure (order {order_p:.2f})")
    ax.set_xlabel("h [m]")
    ax.set_ylabel("error")
    ax.set_title("Wall-bounded MMS convergence")
    ax.grid(alpha=0.3, which="both")
    ax.legend()
    fig.tight_layout()
    fig.savefig(out.with_suffix(".png"), dpi=150)
    fig.savefig(out.with_suffix(".pdf"))
    plt.close(fig)
    return order_u, order_p


def plot_summary(rows: list[dict], out: Path) -> None:
    cases = []
    for r in rows:
        if r["case"] not in cases:
            cases.append(r["case"])
    direct = {r["case"]: r for r in rows if r["solver"] == "direct"}
    fs = {r["case"]: r for r in rows if r["solver"] == "fieldsplit"}
    x = np.arange(len(cases))
    fig, axes = plt.subplots(1, 2, figsize=(10.5, 4.2))
    axes[0].bar(x - 0.2, [direct[c]["wall"] for c in cases], 0.4,
                label="direct", color=COLORS["direct"])
    axes[0].bar(x + 0.2, [fs[c]["wall"] for c in cases], 0.4,
                label="fieldsplit", color=COLORS["fieldsplit"])
    axes[0].set_xticks(x, cases)
    axes[0].set_ylabel("wall time [s]")
    axes[0].set_title("Time")
    axes[0].legend()
    axes[0].grid(alpha=0.3, axis="y")
    axes[1].bar(x - 0.2, [direct[c]["peak"] / 2**20 for c in cases], 0.4,
                label="direct", color=COLORS["direct"])
    axes[1].bar(x + 0.2, [fs[c]["peak"] / 2**20 for c in cases], 0.4,
                label="fieldsplit", color=COLORS["fieldsplit"])
    axes[1].set_xticks(x, cases)
    axes[1].set_ylabel("peak RSS [MB]")
    axes[1].set_title("Memory")
    axes[1].legend()
    axes[1].grid(alpha=0.3, axis="y")
    fig.suptitle("saturnStokes direct vs fieldsplit")
    fig.tight_layout(rect=(0, 0, 1, 0.93))
    fig.savefig(out.with_suffix(".png"), dpi=150)
    fig.savefig(out.with_suffix(".pdf"))
    plt.close(fig)


def run_dimension(name: str, args) -> tuple[list[dict], list[dict], float]:
    cfg = DIMS[name]
    folder = ROOT / cfg["folder"]
    dim = cfg["dim"]
    work = args.out / name
    (work / "config").mkdir(parents=True, exist_ok=True)
    (work / "data").mkdir(parents=True, exist_ok=True)
    (work / "logs").mkdir(parents=True, exist_ok=True)
    (work / "figures").mkdir(parents=True, exist_ok=True)

    compare = cfg["quick"] if args.quick else cfg["compare"]
    if args.quick and dim == 2:
        compare = (96, 96, 1)
    results: list[dict] = []
    mms_series: list[dict] = []
    orders = (0.0, 0.0)

    for label, yaml_name in CASES:
        base = read_simple_yaml(folder / yaml_name)
        case_name = value(base, "stokes", "case", label)
        mesh = compare
        if label == "mmsWall":
            mm = cfg["mms_quick"] if args.quick else cfg["mms"]
            meshes = [(n, n, 1) if dim == 2 else (n, n, n) for n in mm]
        else:
            meshes = [mesh]

        run_sets = []
        if label == "mmsWall":
            for mesh_i in meshes:
                run_sets.append((mesh_i, "direct"))
            run_sets.append((meshes[-1], "fieldsplit"))
        else:
            run_sets.append((mesh, "direct"))
            run_sets.append((mesh, "fieldsplit"))

        loaded = {}
        for mesh_i, solver in run_sets:
            tag = f"{label}_{solver}_"
            if label == "mmsWall" and solver == "direct":
                tag += f"{mesh_i[0]}"
            else:
                tag += "cmp"
            prefix = work / "data" / tag
            cfg_path = work / "config" / f"{tag}.yaml"
            emit_case(cfg_path, base, mesh_i, solver, prefix, args.ranks)
            cmd = [args.cli, str(cfg_path)]
            if args.ranks > 1:
                cmd = [args.mpiexec, "-n", str(args.ranks), *cmd]
            log = work / "logs" / f"{tag}.log"
            print("+", " ".join(cmd), flush=True)
            code, wall, peak = run_case(cmd, log)
            if code != 0:
                raise SystemExit(f"run failed ({code}); see {log}")
            meta, data = load(prefix)
            loaded[(mesh_i, solver)] = (meta, data, wall, peak)
            if label == "mmsWall" and solver == "direct":
                mms_series.append({
                    "mesh": mesh_i[0],
                    "h": 1.0 / mesh_i[0],
                    "l2": float(meta["exact"]["l2"]),
                    "pL2": float(meta["exact"]["pL2"]),
                    "its": int(meta.get("iterations", 0)),
                })

        if label == "mmsWall":
            order_u, order_p = plot_mms_order(
                mms_series, work / "figures" / "mmsWall_order")
            orders = (order_u, order_p)
            prev = mms_series[-1]
            print(f"  wall MMS order: u {order_u:.2f}, p {order_p:.2f}")

        mesh_final = meshes[-1] if label == "mmsWall" else compare
        dmeta, ddata, dwall, dpeak = loaded[(mesh_final, "direct")]
        fmeta, fdata, fwall, fpeak = loaded[(mesh_final, "fieldsplit")]
        vel_diff, p_diff = solver_diff(ddata, fdata)
        cons = exact_consistency(base, ddata, case_name)
        row_direct = {
            "dim": name, "case": label, "solver": "direct",
            "mesh": mesh_final, "its": int(dmeta.get("iterations", 0)),
            "wall": dwall, "peak": dpeak,
            "l2": float(dmeta["exact"]["l2"]),
            "pL2": float(dmeta["exact"]["pL2"]),
            "velDiff": 0.0, "pDiff": 0.0, "consistency": cons,
        }
        row_fs = {
            "dim": name, "case": label, "solver": "fieldsplit",
            "mesh": mesh_final, "its": int(fmeta.get("iterations", 0)),
            "wall": fwall, "peak": fpeak,
            "l2": float(fmeta["exact"]["l2"]),
            "pL2": float(fmeta["exact"]["pL2"]),
            "velDiff": vel_diff, "pDiff": p_diff,
            "consistency": cons,
        }
        results += [row_direct, row_fs]

        if label != "mmsWall":
            if dim == 2:
                plot_2d_validation(dmeta, ddata, label,
                                   work / "figures" / f"{label}_validation")
            else:
                plot_3d_validation(dmeta, ddata, label,
                                   work / "figures" / f"{label}_validation")
        plot_solver_bars([row_direct, row_fs],
                         f"saturnStokes {name} {label} "
                         f"{mesh_final[0]}x{mesh_final[1]}"
                         + (f"x{mesh_final[2]}" if dim == 3 else ""),
                         work / "figures" / f"{label}_solvers")

    plot_summary(results, work / "figures" / "summary_solvers")
    return results, mms_series, orders


def write_summary(results: list[dict], orders: dict[str, tuple[float, float]],
                  out: Path) -> bool:
    lines = [
        f"{'dim':4s} {'case':10s} {'solver':10s} {'its':>4s} "
        f"{'wall[s]':>9s} {'RSS[MB]':>9s} {'L2':>11s} {'pL2':>11s} "
        f"{'|dfs-dir|':>11s} {'p-diff':>11s}",
    ]
    ok = True
    for r in results:
        lines.append(
            f"{r['dim']:4s} {r['case']:10s} {r['solver']:10s} "
            f"{r['its']:4d} {r['wall']:9.3f} {r['peak'] / 2**20:9.1f} "
            f"{r['l2']:11.4e} {r['pL2']:11.4e} {r['velDiff']:11.3e} "
            f"{r['pDiff']:11.3e}"
        )
        if r["solver"] == "fieldsplit":
            if r["velDiff"] > 1e-6 or r["pDiff"] > 1e-5:
                ok = False
            if r["case"] != "mmsWall" and r["l2"] > 1e-3:
                ok = False
    for dim, (ou, op) in orders.items():
        lines.append(f"{dim} wall MMS order: velocity {ou:.2f}, "
                     f"pressure {op:.2f}")
        if ou < 1.8 or (dim == "2d" and op < 1.5):
            ok = False
    for label in ("poiseuille", "couette"):
        vals = [r["consistency"] for r in results
                if r["case"] == label and r["solver"] == "direct"]
        if vals:
            lines.append(f"{label}: |u_exact - independent analytic|max = "
                         f"{max(vals):.3e}")
            if max(vals) > 1e-10:
                ok = False
    text = "\n".join(lines)
    out.write_text(text + "\n", encoding="utf-8")
    print("\n" + text)
    return ok


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dim", choices=["2d", "3d", "both"], default="both")
    parser.add_argument("--cli", default="saturnCFD")
    parser.add_argument("--mpiexec", default="mpiexec")
    parser.add_argument("--ranks", type=int, default=1)
    parser.add_argument("--quick", action="store_true")
    parser.add_argument("--out", type=Path, default=ROOT / "results")
    args = parser.parse_args()

    dims = ["2d", "3d"] if args.dim == "both" else [args.dim]
    all_results: list[dict] = []
    orders: dict[str, tuple[float, float]] = {}
    for name in dims:
        results, _, order = run_dimension(name, args)
        all_results += results
        orders[name] = order

    args.out.mkdir(parents=True, exist_ok=True)
    ok = write_summary(all_results, orders, args.out / "summary.txt")
    print(f"\nsummary : {args.out / 'summary.txt'}")
    print(f"figures : {args.out}/{{2d,3d}}/figures/")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
