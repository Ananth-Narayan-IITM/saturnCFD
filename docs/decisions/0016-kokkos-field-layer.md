# 0016 — Kokkos-backed field and FVM layer

- **Status:** accepted (first slice: CellField, FieldHalo, Cartesian Laplacian,
  periodic MMS; general LocalMesh and remaining operators follow)
- **Date:** 2026-10-31
- **Owner:** saturnCFD v1
- **Supersedes:** none
- **Protocol:** `docs/protocols/parallel.md`, `docs/protocols/finiteVolume.md`
- **Tests:** `tests/fv/...`, `tests/TestParallel.cpp`, `benchmarks/...`

## Context

The MPI partition/halo layer is host-staged and tested. The next step is a real
ghosted field that can be filled on the device and used by FVM kernels on CPU
and GPU. This is where the field layout and kernel policy are fixed.

## Decision

1. **`CellField<Real>` owns one ghosted Kokkos view** with `ncomp`, `nGrow`,
   local valid origin and extents. Global `(i,j,k,c)` access subtracts the
   local ghost origin.
2. **Vector fields use three components** in a `CellField`; no separate Vector
   view type.
3. **`LocalMesh`/`DeviceGeometry`** owns cell volumes/centroids and face
   areas/normals/centroids in Kokkos views, copied once before the time loop.
4. **Halo exchange gets a Kokkos overload**: device pack → host mirror → MPI →
   device unpack. The existing host path remains for tests/CPU fallback.
5. **Kernels iterate owned cells only**; ghosts are read-only. No `this`
   capture, no allocation, no MPI inside a kernel.
6. **MDRange first**, flat range benchmarked later.
7. **Temporary face fluxes first**; a persistent `FaceField` is added when
   pressure correction requires it.
8. **CPU and GPU are separate builds** (`cpu`/`gpu` presets). MPI is ON in both.
9. **One rank per GPU** for performance; multiple ranks on one GPU are
   correctness tests only.

## Alternatives considered

| alternative | why not chosen |
|---|---|
| Host `std::vector` fields | no GPU path; would be rewritten immediately |
| One global Kokkos view per rank | no ghost layout; halo copy becomes awkward |
| Separate `VectorCellField` memory | unnecessary duplication |
| Face field from day one | more ownership complexity before pressure needs it |
| MPI calls inside kernels | illegal/deadlock-prone |
| GPU-aware MPI first | not portable; host staging is correct first |

## Consequences

- **Code:** `src/fv/CellField.hpp`, `LocalMesh`, halo overload,
  `gradient/divergence/laplacian`.
- **Build:** Kokkos becomes a first-class dependency; CPU/GPU presets.
- **Testing:** CPU/GPU parity, 1/2/4-rank halo and FVM equality.
- **Risk:** MDRange performance on GPU must be benchmarked; flat range is the
  fallback.

## Validation / exit criteria

- `CellField` fill/reduce works on CPU and GPU.
- Halo of a `CellField` matches the analytic periodic field on 1/2/4 ranks.
- Constant/linear-field FVM tests pass.
- Manufactured Poisson shows the expected order.
- CPU/GPU parity within the precision floor.
- `mpirun -np 1` GPU runs the field/halo test.

## References

- ADR `0014`, `0015`.
- `docs/plan/fvmKokkosPlan.md`.
- AMReX `MultiFab`/`Array4` design (methodology reference).
