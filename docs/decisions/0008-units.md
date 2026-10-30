# 0008 — Units and dimensional consistency

- **Status:** proposed
- **Date:** 2026-10-30
- **Owner:** saturnCFD v1
- **Supersedes:** none
- **Protocol:** `docs/theory/units.md`
- **Tests:** `tests/core/units...` (later)

## Context

The user wants the whole solver to be dimensionally correct, but not to spend
the first phase on a units library. The risk is that raw `Real` values mix
metres, seconds, pascals and dimensionless Reynolds numbers, and that adding
units later forces a revalidation of every case.

## Decision (recommended)

1. **Use SI internally and in YAML from the start.** Mesh coordinates in
   metres, velocity in m/s, pressure in Pa, density in kg/m³, dynamic viscosity
   in Pa·s, temperature in K, time in s.
2. **Store dynamic pressure \(p\) [Pa]**, not kinematic pressure, so the state
   is ready for variable-density and two-phase flow. For constant density the
   momentum equation is divided by \(\rho\); document that operation.
3. **No mixed dimensionless runs.** Benchmark cases use SI values with
   \(L=1\,\mathrm{m}\), \(U=1\,\mathrm{m/s}\), \(\rho=1\,\mathrm{kg/m^3}\) and
   \(\mu=1/Re\,\mathrm{Pa\,s}\). The Reynolds number is a derived diagnostic,
   not a hidden unit system.
4. **Centralise conversions.** All physical properties live in a
   `PhysicalProperties` struct; mesh scaling lives in one reader option
   (`mesh.scale`, default 1.0). No scattered `* 1000` or `/ rho`.
5. **Reserve a `units:` block** in the schema for later parsing of values such
   as `1.0 cm`, `1.0e-3 Pa s`. It is not implemented in v1.0.
6. **Design for a units layer at the boundary.** When the solvers are stable,
   add `mp-units` or a small `Quantity<Dimension>` wrapper to config and I/O
   only; convert to SI before any Kokkos kernel. Do not put unit types in hot
   kernels.
7. **Document both forms.** Every theory page gives the dimensional equation
   and, where useful, its dimensionless form with the reference scales.

## Why not wait entirely

Adding units after all four solvers are validated forces a re-run of every
validation case and can change empirical constants. SI from the start is almost
free and removes that risk. A full units library can wait, but the **naming,
property structs and conversion points** should exist now.

## Alternatives considered

| alternative | why not chosen |
|---|---|
| Fully dimensionless internally | compact for incompressible benchmarks, but bad fit for compressible/two-phase and hides unit errors |
| Full units library now | compile-time and GPU complexity before the physics is stable |
| Raw `Real` with no convention | the exact failure mode the user wants to avoid |
| SI everywhere but no `PhysicalProperties` | conversions scatter and become inconsistent |

## Consequences

- **Code:** `PhysicalProperties`, `Scales` (reserved), `mesh.scale`; no unit
  types in kernels.
- **YAML:** all values are SI; comments state the unit.
- **Validation:** every analytic case is written in SI and checked
  dimensionally before implementation.
- **Risk:** users may supply CGS or mm; `mesh.scale` and the future `units:`
  block are the mitigation.

## Validation / exit criteria

- The Burgers traveling wave uses \(U\) [m/s], \(\nu\) [m²/s] and the argument
  \(U(x-Ut/2)/(4\nu)\) is dimensionless.
- The heat boundary-layer solution uses \(u x/\alpha\) as a dimensionless
  exponent.
- The Stokes Poiseuille profile has units of m/s.
- The run manifest records the unit convention.

## References

- SI Brochure, 9th edition (BIPM).
- `mp-units` documentation (future work).
