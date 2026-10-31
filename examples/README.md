# examples

Case files that a user can run directly. One directory per solver, camelCase
case names.

```text
examples/
  saturnBurger/
    saturnBurger.yaml      # 1D viscous traveling wave
    compareAnalytic.py     # independent analytic comparison and plot
    README.md
  heat/                    # later
  saturnStokes/            # later
  saturnIncompressible/    # later
```

Each case is minimal and documented at the top. The larger validation cases
live in `tests/regression/` so that the example directory stays small.

## Rules

- All values are SI.
- No hidden defaults: a case that relies on a default says so in a comment.
- A new solver adds at least one example.
- `-checkConfig` must pass on every example.
- Generated output goes under `results/` (ignored by git).

## Burgers

```bash
cd saturnBurger
saturnCFD saturnBurger.yaml
python3 compareAnalytic.py
```

The script recomputes the exact traveling wave from the case parameters and
prints the L1/L2/Linf error, a PASS/FAIL check and a comparison plot. See
[`saturnBurger/README.md`](saturnBurger/README.md).
