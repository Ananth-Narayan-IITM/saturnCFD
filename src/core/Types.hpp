/**
 * @file Types.hpp
 *
 * saturnCFD — core type aliases. Everything downstream is written in terms of
 * these, so switching dimension or precision is a configure-time decision.
 */
#pragma once

#include <Kokkos_Array.hpp>
#include <Kokkos_Core.hpp>

#include <cstddef>

#ifndef SATURN_NDIM
#define SATURN_NDIM 2
#endif

namespace saturn {

constexpr int NDIM = SATURN_NDIM;
static_assert(NDIM == 2 || NDIM == 3, "saturnCFD supports 2D and 3D only");

/// Floating point type for field data. Toggle with -DSATURN_PRECISION=float.
#ifdef SATURN_REAL_DOUBLE
using Real = double;
#else
using Real = float;
#endif

/// Always use double for reductions (residuals, norms, timings). Even in float
/// mode: a float accumulation of 1e6 terms tells you nothing.
using Accum = double;

using Index = int;
using BigIndex = long long;

/// Multi-dimensional index. Always 3-wide so that 2D code can still name index
/// 2 (always 0) without #ifdefs; loops are bounded by NDIM.
using Idx = Kokkos::Array<Index, 3>;

/// Execution / memory spaces. Driven entirely by how Kokkos was configured.
using ExecSpace = Kokkos::DefaultExecutionSpace;
using MemSpace = ExecSpace::memory_space;
using Layout = Kokkos::LayoutRight;

/// NOTE the explicit '*' : in Kokkos, View<T, MemSpace> is a RANK-0 scalar
/// view.
template <class T>
using View = Kokkos::View<T*, MemSpace>;

template <class T>
using HostView = Kokkos::View<T*, Kokkos::HostSpace>;

/// Device-resident scratch, used for per-thread temporaries.
using ScratchSpace = typename ExecSpace::scratch_memory_space;

inline constexpr int NDIM3 =
    3;  // internal strides always 3D; NDIM==2 uses nz=1

}  // namespace saturn
