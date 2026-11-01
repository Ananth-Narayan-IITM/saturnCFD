# Coupled saddle-point solver

**Status:** design accepted for `saturnStokes`; the same block is the
pressure–velocity core of `saturnIncompressible`.

## 1. Why a coupled solve

Steady Stokes flow is linear:

$$
-\mu\nabla^2\mathbf{u}+\nabla p=\mathbf{f},
\qquad
\nabla\cdot\mathbf{u}=0 .
$$

The pressure is a Lagrange multiplier for continuity, so velocity and pressure
must be solved together. A segregated method such as SIMPLE or SIMPLEC adds an
outer pressure-correction iteration and under-relaxation. For Stokes those
iterations are unnecessary: one coupled linear solve gives both fields.

The coupled method also gives the reusable foundation for incompressible
Navier–Stokes, where the same saddle-point block appears inside a Picard,
Newton or time-step iteration. SIMPLE/SIMPLEC remain useful alternatives for
steady Navier–Stokes, but they are not the primary Stokes method.

## 2. Block system

After discretisation the unknowns are the cell-centred velocity
$\mathbf{u}_h$ and pressure $p_h$. The linear system is

$$
\left[\begin{array}{cc}
A & B^{T}\\
B & 0
\end{array}\right]
\left[\begin{array}{cc}
\mathbf{u}_h\\ p_h
\end{array}\right]
=
\left[\begin{array}{cc}
\mathbf{f}_h\\ \mathbf{g}_h
\end{array}\right],
$$

where

- $A$ is the vector viscous operator, with Dirichlet wall rows;
- $B$ is the discrete divergence;
- $B^{T}$ is the discrete pressure gradient;
- $\mathbf{f}_h$ contains body forces and boundary data;
- $\mathbf{g}_h$ is the target continuity residual (zero for an
  incompressible case).

The matrix is symmetric indefinite. For an all-Neumann pressure boundary the
constant pressure mode is in the null space; the solver must attach that mode
and either fix a reference value or keep the null space.

## 3. Collocated finite-volume discretisation

The first implementation keeps the existing cell-centred fields:

- $\mathbf{u}_P$ and $p_P$ live at cell centres;
- momentum rows use the component-wise Laplacian on `LocalMesh`;
- the pressure gradient is the cell-centred Gauss gradient;
- the continuity row uses a **Rhie–Chow** face-normal velocity:
  $$
  u_{f} = \overline{\mathbf{u}}_f\cdot\mathbf{n}_f
  - d_f\left[
  \frac{p_N-p_P}{d_{PN}}
  - \overline{\nabla p}_f\cdot\mathbf{n}_f
  \right].
  $$

The Rhie–Chow correction removes the odd–even pressure mode of a collocated
grid and supplies the pressure coupling needed by the saddle-point system. The
correction coefficient $d_f$ is based on the momentum diagonal (or a documented
scalar approximation in the first version). The resulting discrete system is a
**stabilised saddle-point system**: its continuity block is not exactly $B$ of
the continuous problem, but its pressure null space and the velocity/pressure
block structure are retained.

Boundary conditions:

- no-slip and moving walls: Dirichlet velocity rows;
- inlet: fixed velocity profile;
- outlet: fixed pressure or zero-gradient velocity;
- periodic patches: wrapped neighbours;
- all-Neumann pressure: constant null space, mean pressure reported.

## 4. PETSc linear solver

The production path is PETSc, not a home-grown Krylov method. The current
`saturnStokes` validation core uses PETSc's coupled direct LU
(`preonly` + `lu`); the fieldsplit/Schur path below is implemented as an option
and is still being tuned for refined meshes.

### Matrix and vectors

- `MPIAIJ` matrix assembled from the `LocalMesh` face coefficients.
- Four degrees of freedom per cell in 3D (`u`, `v`, `w`, `p`), three in 2D.
- A local-to-global map covers owned cells and one ghost layer, so boundary and
  partition faces can be assembled with `MatSetValues` and `VecSetValues`.
- The pressure block reuses the scalar Laplacian sparsity pattern.
- Matrix and preconditioner setup are reused when the coefficients do not
  change. For Navier–Stokes the matrix is rebuilt or updated each outer
  iteration.

### Outer Krylov method

Use **GMRES** (or FGMRES when the preconditioner is variable) on the coupled
system. The outer tolerance is set explicitly, never left at the PETSc default
in a validation run. Record the iteration count, preconditioned residual and
true residual.

### Fieldsplit / Schur preconditioner

The pressure–velocity block is solved with PETSc `PCFIELDSPLIT`. The basic
factorisation is

$$
\left[\begin{array}{cc}
A & B^{T}\\
B & 0
\end{array}\right]
=
\left[\begin{array}{cc}
A & 0\\
B & S
\end{array}\right]
\left[\begin{array}{cc}
I & A^{-1}B^{T}\\
0 & I
\end{array}\right],
\qquad
S=-B A^{-1}B^{T}.
$$

The preconditioner applies an approximate inverse of this factorisation:

1. an approximate velocity solve with $A$ (AMG or ILU);
2. an approximate Schur solve with $\widehat S$ (GAMG, HYPRE BoomerAMG, or a
   pressure Laplacian scaled by the momentum diagonal);
3. a back-substitution for the pressure coupling.

This is the standard **Schur-complement fieldsplit** idea. The exact inverse is
never formed; the preconditioner only needs a spectrally equivalent
approximation. GMRES handles the remaining coupling.

### Recommended PETSc settings

| block | KSP | PC | notes |
|---|---|---|---|
| coupled outer | GMRES/FGMRES | fieldsplit | explicit `rtol`, `atol`, `max_it` |
| velocity split | preonly | GAMG or ILU | vector Laplacian, one V-cycle |
| pressure split | preonly | GAMG or HYPRE BoomerAMG | scalar pressure Laplacian |
| small validation | preonly | LU / MUMPS / SuperLU | direct reference solution |

Typical options:

```text
-pc_type fieldsplit
-pc_fieldsplit_type schur
-pc_fieldsplit_schur_fact_type full
-pc_fieldsplit_schur_precondition self
-fieldsplit_velocity_ksp_type preonly
-fieldsplit_velocity_pc_type gamg
-fieldsplit_pressure_ksp_type preonly
-fieldsplit_pressure_pc_type gamg
-ksp_type fgmres
-ksp_rtol 1e-10
```

For all-Neumann pressure, create `MatNullSpace` with the constant vector and
attach it with `KSPSetNullSpace`; do not guess a reference cell. The pressure
mean is removed for error norms and reported separately.

## 5. Navier–Stokes extension

For incompressible Navier–Stokes,

$$
\frac{\partial\mathbf{u}}{\partial t}
+\nabla\cdot(\mathbf{u}\otimes\mathbf{u})
+\nabla p-\mu\nabla^2\mathbf{u}=\mathbf{f},
\qquad
\nabla\cdot\mathbf{u}=0,
$$

the same block appears after linearising the convection term. A Picard
iteration gives

$$
\left[\begin{array}{cc}
A+C(\mathbf{u}^{k}) & B^{T}\\
B & 0
\end{array}\right]
\left[\begin{array}{cc}
\delta\mathbf{u}\\ \delta p
\end{array}\right]
=
-\left[\begin{array}{cc}
R_u\\ R_p
\end{array}\right],
$$

where $C$ is the linearised convection operator. A Newton iteration adds the
velocity-gradient dependence of $C$. For time-dependent flow, add the mass
matrix $M/\Delta t$ to the velocity block.

The practical path is:

1. Picard/Newton with the coupled saddle-point solve for steady Navier–Stokes;
2. semi-implicit time stepping with the same block per step;
3. SIMPLE/SIMPLEC as an optional segregated path for cases where the coupled
   solve is too expensive.

## 6. Validation and test matrix

| test | what it checks | acceptance |
|---|---|---|
| Poiseuille 2D/3D | parabolic velocity, constant pressure gradient | machine-order profile error |
| Couette 2D/3D | linear velocity, constant pressure | machine-order profile error |
| divergence | discrete continuity | `||div u||` below solver tolerance |
| MMS 2D/3D | velocity and pressure order | second order in $h$ |
| pressure null space | constant mode and mean | no drift, mean reported |
| checkerboard | odd–even pressure | suppressed by Rhie–Chow |
| fieldsplit | Schur preconditioner | GMRES converges on 1/2/4 ranks |
| exit boundary | BEM paper cases | quantitative comparison |

## References

- Benzi, Golub & Liesen (2005), *Numerical solution of saddle point problems*.
- Elman, Silvester & Wathen (2014), *Finite Elements and Fast Iterative
  Solvers*.
- Kay, Loghin & Wathen (2002), preconditioner for the steady-state
  Navier–Stokes equations.
- Elman, Howle, Shadid, Shuttleworth & Tuminaro (2006), block preconditioners
  based on approximate commutators.
- Rhie & Chow (1983), collocated pressure–velocity coupling.
- Balay et al., *PETSc/TAO Users Manual*.
- Sankhla & Das (2025), 3D steady Stokes with exit boundary conditions (test
  cases).
