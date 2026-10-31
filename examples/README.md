# examples

Case files that a user can run directly. The first level is the solver; the
next level is the dimensionality.

```text
examples/
  saturnBurger/
    saturnBurger1D/        # 1D viscous traveling wave
      saturnBurger.yaml
      compareAnalytic.py
      README.md
    saturnBurger2D/        # 2D diagonal traveling wave
      saturnBurger2D.yaml
      compareAnalytic.py
      README.md
  heat/                    # later
  saturnStokes/            # later
  saturnIncompressible/    # later
```

Each case is minimal and documented at the top. The larger validation cases
live in `tests/regression/` so that the example directory stays small. Examples
are user-editable demos: CTest validates their schema, while numerical
regressions use the stable files under `tests/regression/`.

## Rules

- All values are SI.
- No hidden defaults: a case that relies on a default says so in a comment.
- A new solver or dimension adds at least one example.
- `-checkConfig` must pass on every example.
- Generated output goes under `results/` (ignored by git).

## Running the Burgers examples

```bash
# 1D
cd saturnBurger/saturnBurger1D
saturnCFD saturnBurger.yaml
python3 compareAnalytic.py

# 2D
cd ../saturnBurger2D
saturnCFD saturnBurger2D.yaml
python3 compareAnalytic.py
```

Each script recomputes the analytic solution from the case parameters and
prints L1/L2/Linf, a PASS/FAIL check and a comparison plot. See the README in
each dimension directory.
