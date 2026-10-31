# Poisson equation and the PETSc matrix

**Status:** linear-algebra smoke tests implemented. The finite-volume matrix
assembler from `LocalMesh` is the next step.

## 1. Continuous problem

On a domain $\Omega$:

$$
-\nabla\cdot(\Gamma\nabla u) = f \quad \mathrm{in}\ \Omega,
$$

with Dirichlet or Neumann data on each boundary patch. For a constant
coefficient and a uniform grid this is the model problem used by the PETSc
tests.

## 2. Finite-volume matrix

For a cell $P$ with volume $V_P$, face area $A_f$, outward normal $n_f$ and
owner-to-face normal distance $d_f$:

$$
\sum_f -\Gamma A_f \frac{u_N-u_P}{d_f} = V_P f_P .
$$

For an internal face:

$$
A_{PP} \mathrel{+}= \frac{\Gamma A_f}{d_f},\qquad
A_{PN} \mathrel{-}= \frac{\Gamma A_f}{d_f}.
$$

The matrix is symmetric positive definite for Dirichlet data and has the
constant nullspace for all-Neumann data.

### Dirichlet face

The ghost value is $u_g = 2g_D-u_P$, so the boundary flux is
$\Gamma A_f (u_P-g_D)/d_f$. Therefore:

$$
A_{PP} \mathrel{+}= \frac{\Gamma A_f}{d_f},\qquad
b_P \mathrel{+}= \frac{\Gamma A_f}{d_f}g_D .
$$

No off-diagonal is created for the ghost.

### Neumann face

The prescribed outward normal derivative is $g_N=\partial_n u$:

$$
-\Gamma A_f g_N \quad\mathrm{on\ the\ LHS}\qquad\Longrightarrow\qquad
b_P \mathrel{+}= \Gamma A_f g_N .
$$

No matrix contribution is added. The sign convention is the outward normal
derivative; document it in every case file.

### All-Neumann nullspace

The matrix is singular. Attach the constant vector:

```cpp
MatNullSpaceCreate(comm, PETSC_TRUE, 0, nullptr, &nullspace);
MatSetNullSpace(A, nullspace);
```

The right-hand side must be compatible (zero mean); report the pressure mean
as a diagnostic.

## 3. Non-orthogonal correction

The current `LocalMesh` Laplacian has a one-pass tangential correction. For an
implicit matrix, keep only the orthogonal coefficient in `A` and put the
correction on the RHS:

$$
b_P \mathrel{-}= \Gamma A_f
\frac{\mathbf{d}\cdot\nabla u_f^{t}}{d_\perp}.
$$

This preserves symmetry and allows CG/AMG. Iterate the correction once or
twice per time step for strongly non-orthogonal meshes.

## 4. Solver

- Dirichlet/heat: CG + GAMG.
- Nonsymmetric convection-dominated: GMRES + ILU/AMG.
- All-Neumann: CG + GAMG with `MatNullSpace`.
- Small validation meshes: a direct solver is acceptable.

The PETSc wrapper is `saturn::KspSolver` in `src/linalg/Petsc.hpp`.

## 5. Tests

The Phase 0 tests use PETSc `DMDA` only to verify the linear algebra path:

| case | grid | solver | observed |
|---|---|---|---|
| 1D Dirichlet Poisson | 17, 33, 65 nodes | CG + Jacobi | order 1.98, 1.99 |
| 3D Dirichlet Poisson | $12^3$ | CG + GAMG | L2 error $2.1\times10^{-3}$ |
| 1D all-Neumann | 17 nodes | CG + GAMG + nullspace | nullspace test passes |

These are finite-difference smoke tests. The production matrix will use the
same stencil coefficients above, assembled from `LocalMesh`.

## 6. Production assembly plan

1. Build a local row numbering and a global PETSc ownership map.
2. Assemble internal-face coefficients from `LocalMesh`.
3. Apply Dirichlet/Neumann rows and RHS.
4. Attach nullspace for all-Neumann pressure.
5. Solve with `KspSolver`.
6. Verify 1D/3D order and 1/2/4-rank equality.

## References

- Patankar (1980), *Numerical Heat Transfer and Fluid Flow*.
- Wesseling (2001), *Principles of Computational Fluid Dynamics*.
- PETSc/TAO Users Manual.
- ADR `0003`, `0017`.
