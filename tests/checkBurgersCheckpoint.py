#!/usr/bin/env python3
"""Run a checkpoint/restart round trip for saturnBurger.

Runs the same case once to t=0.2 (reference), once to t=0.1 with checkpoints,
then resumes from the step-5 checkpoint to t=0.2. The restart final norms must
match the reference run.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path


def run(cli: Path, case: Path, cwd: Path) -> None:
    print("+", cli, case.name)
    result = subprocess.run(
        [str(cli), str(case)],
        cwd=cwd,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if result.returncode != 0:
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise RuntimeError(f"case failed: {case}")


def summary(path: Path) -> dict:
    return json.loads(path.read_text())


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cli", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--full", type=Path, required=True)
    parser.add_argument("--checkpoint", type=Path, required=True)
    parser.add_argument("--template", type=Path, required=True)
    args = parser.parse_args()

    out = args.out.resolve()
    if out.exists():
        import shutil

        shutil.rmtree(out)
    out.mkdir(parents=True)

    cli = args.cli.resolve()
    run(cli, args.full.resolve(), out)
    run(cli, args.checkpoint.resolve(), out)

    checkpoints = sorted((out / "results/burgers").glob("checkpoint_*.chk"))
    if not checkpoints:
        raise SystemExit("no checkpoint files were written")
    chosen = next(
        (p for p in checkpoints if p.stem.endswith("_000005")),
        checkpoints[0],
    )

    text = args.template.read_text().replace("@CHECKPOINT@", str(chosen))
    restart_case = out / "restart.yaml"
    restart_case.write_text(text)
    run(cli, restart_case, out)

    full = summary(out / "results/burgers/full.json")
    restart = summary(out / "results/burgers/restart.json")
    full_exact = full["exact"]
    restart_exact = restart["exact"]

    print()
    print(f"checkpoint : {chosen}")
    print(f"full   L2  : {full_exact['l2']:.16e}")
    print(f"restart L2 : {restart_exact['l2']:.16e}")
    print(f"restart step: {restart.get('restartStep', 'missing')}")

    if restart.get("restartStep") != 5:
        raise SystemExit("restart did not resume from step 5")
    for key in ("l1", "l2", "linf"):
        a = float(full_exact[key])
        b = float(restart_exact[key])
        relative = abs(a - b) / max(1.0e-30, abs(a))
        if relative > 1.0e-10:
            raise SystemExit(
                f"{key} mismatch: reference {a:.16e}, restart {b:.16e}, "
                f"relative {relative:.3e}"
            )
    print("checkpoint/restart OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
