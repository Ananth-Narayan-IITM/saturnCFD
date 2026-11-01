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
    saturnBurger3D/        # 3D diagonal traveling wave
      saturnBurger3D.yaml
      compareAnalytic.py
      README.md
  saturnHeat/
    saturnHeat2D/          # 2D periodic Fourier convection–diffusion mode
      saturnHeat2D.yaml
      compareAnalytic.py
      README.md
    saturnHeat3D/          # 3D periodic Fourier convection–diffusion mode
      saturnHeat3D.yaml
      compareAnalytic.py
      README.md
  saturnStokes/
    saturnStokes2D/        # 2D Poiseuille/Couette coupled Stokes
      poiseuille.yaml
      couette.yaml
      compareAnalytic.py
      README.md
    saturnStokes3D/        # 3D extruded Poiseuille/Couette
      poiseuille.yaml
      couette.yaml
      compareAnalytic.py
      README.md
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

# 3D
cd ../saturnBurger3D
saturnCFD saturnBurger3D.yaml
python3 compareAnalytic.py
```

Each script recomputes the analytic solution from the case parameters and
prints L1/L2/Linf, a PASS/FAIL check and a comparison plot. See the README in
each dimension directory.

## Running the heat examples

```bash
# 2D periodic Fourier mode
cd saturnHeat/saturnHeat2D
saturnCFD saturnHeat2D.yaml
python3 compareAnalytic.py

# 3D periodic Fourier mode
cd ../saturnHeat3D
saturnCFD saturnHeat3D.yaml
python3 compareAnalytic.py
```

The heat scripts compare against
`T = A sin(k.x - (a.k)t) exp(-alpha |k|^2 t)` and draw the numerical field,
the analytic field, the error and a cut/diagonal profile.

## Running the Stokes examples

```bash
cd saturnStokes/saturnStokes2D
saturnCFD poiseuille.yaml
python3 compareAnalytic.py --case poiseuille
python3 compareAnalytic.py --case couette

cd ../saturnStokes3D
saturnCFD poiseuille.yaml
python3 compareAnalytic.py --case poiseuille
python3 compareAnalytic.py --case couette
```

The Stokes scripts compare the numerical velocity with the exact Poiseuille
parabola or Couette line and plot the field, error and averaged profile.
