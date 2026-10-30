# Units and dimensional consistency

**Status:** proposed (ADR `0008`)

The solver uses **SI internally and in YAML**. There is no hidden
non-dimensionalisation in v1.0. Benchmark cases use SI values that happen to
correspond to the usual dimensionless set-up.

## 1. Conventions

| quantity | symbol | SI unit |
|---|---|---|
| length, mesh coordinates | \(x,y,z\) | m |
| time | \(t\) | s |
| velocity | \(\mathbf{u}\) | m/s |
| dynamic pressure | \(p\) | Pa = kg/(m·s²) |
| density | \(\rho\) | kg/m³ |
| dynamic viscosity | \(\mu\) | Pa·s |
| kinematic viscosity | \(\nu=\mu/\rho\) | m²/s |
| temperature | \(T\) | K |
| thermal diffusivity | \(\alpha=k/(\rho c_p)\) | m²/s |
| body force per volume | \(\mathbf{f}\) | N/m³ |
| cell volume | \(V_P\) | m³ |
| face area | \(A_f\) | m² |

Mesh coordinates are multiplied by `mesh.scale` once at read time, so a mesh in
millimetres can be imported with `scale: 1.0e-3`.

## 2. Dynamic pressure, not kinematic pressure

v1 stores

\[
p \;[\mathrm{Pa}],
\]

not \(p/\rho\). The incompressible momentum equation is

\[
\rho\left(\frac{\partial \mathbf{u}}{\partial t}
+ \nabla\cdot(\mathbf{u}\otimes\mathbf{u})\right)
= -\nabla p + \mu\nabla^2\mathbf{u} + \mathbf{f},
\]

or, divided by \(\rho\),

\[
\frac{\partial \mathbf{u}}{\partial t}
+ \nabla\cdot(\mathbf{u}\otimes\mathbf{u})
= -\frac{1}{\rho}\nabla p + \nu\nabla^2\mathbf{u}
+ \frac{\mathbf{f}}{\rho}.
\]

The pressure-correction equation is solved for a pressure correction \(p'\)
with units of Pa; the velocity correction is

\[
\mathbf{u} \leftarrow \mathbf{u} - \frac{\Delta t}{\rho}\nabla p'.
\]

Storing dynamic pressure keeps the state ready for variable density and
two-phase flow, where \(p/\rho\) is not a useful single variable.

## 3. Benchmarks in SI

For a lid-driven cavity with \(L=1\,\mathrm{m}\), \(U=1\,\mathrm{m/s}\),
\(\rho=1\,\mathrm{kg/m^3}\):

\[
Re = \frac{\rho U L}{\mu}
\quad\Longrightarrow\quad
\mu = \frac{\rho U L}{Re}.
\]

For \(Re=100\), \(\mu=0.01\,\mathrm{Pa\,s}\), \(\nu=0.01\,\mathrm{m^2/s}\).
The numerical values match the usual dimensionless case because the reference
scales are one. The Reynolds number is a **derived diagnostic**:

```text
Re = rho * U * L / mu
```

It is not a separate unit system.

## 4. Burgers and heat

Burgers:

\[
\frac{\partial u}{\partial t}
+ \frac{\partial}{\partial x}\left(\frac{u^2}{2}\right)
= \nu \frac{\partial^2 u}{\partial x^2},
\]

with \(u\) [m/s], \(\nu\) [m²/s]. The traveling wave is

\[
u(x,t) = \frac{U}{2}\left[
1 - \tanh\left(\frac{U(x-Ut/2)}{4\nu}\right)
\right],
\]

whose argument is dimensionless.

Heat:

\[
\frac{\partial T}{\partial t}
+ \nabla\cdot(\mathbf{u}T)
= \nabla\cdot(\alpha\nabla T) + S,
\]

with \(T\) [K], \(\alpha\) [m²/s], \(S\) [K/s].

## 5. What not to do

- Do not mix a dimensionless velocity with a dimensional pressure in one case.
- Do not hard-code \(\rho=1\) without writing it in the case file.
- Do not scatter conversion factors through kernels. Conversions belong in
  `PhysicalProperties`, `mesh.scale`, and the future `units:` parser.
- Do not add a units library to hot kernels; convert at the boundary.

## 6. Future units layer

When the solvers are stable, add an optional `units:` block:

```yaml
units:
  length: cm
  velocity: m/s
  pressure: Pa
```

and parse values such as `1.0 cm`, `1.0e-3 Pa s`. Internally everything remains
SI. `mp-units` or a small `Quantity<Dimension>` wrapper can be used at the
config and I/O boundary only.

## References

- BIPM, *SI Brochure*, 9th edition.
- `mp-units` documentation (future work).
