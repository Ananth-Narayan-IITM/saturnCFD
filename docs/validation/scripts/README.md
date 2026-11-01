# Validation scripts

Scripts that regenerate the validation tables and figures under
`docs/validation/`. They run the real CLI over several resolutions and are
deliberately not part of CTest: they take minutes and write figures, while the
CTest slice keeps a small, fast regression.

## `validateBurgers.py`

Runs the 1D scalar Burgers traveling-wave case at several resolutions and MPI
rank counts, then writes:

- `docs/validation/figures/burgersValidation.png` / `.pdf`
- convergence tables in the run directory (`results/burgers/validation` by
  default)

From the repository root, with `buildCPU` already compiled:

```bash
python3 docs/validation/scripts/validateBurgers.py
python3 docs/validation/scripts/validateBurgers.py --help
```

The figure is embedded in the Beamer slides and linked from
[`../burgers.md`](../burgers.md).
