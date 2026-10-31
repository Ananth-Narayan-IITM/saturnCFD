#include <doctest/doctest.h>

#include <cmath>
#include <iomanip>
#include <limits>
#include <vector>

#include <Kokkos_Core.hpp>

#ifdef SATURN_USE_MPI
#include <mpi.h>
#endif

#include "comm/HaloExchange.hpp"
#include "comm/Partition.hpp"
#include "fv/BoundaryFill.hpp"
#include "fv/CellField.hpp"
#include "fv/Convection.hpp"
#include "fv/FaceField.hpp"
#include "fv/FieldHalo.hpp"
#include "fv/Laplacian.hpp"
#include "fv/LocalMesh.hpp"
#include "fv/Operators.hpp"

#include "UniformMesh.hpp"

using namespace saturn;

namespace {

KOKKOS_INLINE_FUNCTION constexpr double pi() {
  return 3.14159265358979323846;
}

double globalSum(double value) {
#ifdef SATURN_USE_MPI
  double out = 0;
  MPI_Allreduce(&value, &out, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  return out;
#else
  return value;
#endif
}

double globalMax(double value) {
#ifdef SATURN_USE_MPI
  double out = 0;
  MPI_Allreduce(&value, &out, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
  return out;
#else
  return value;
#endif
}

using Policy3 = Kokkos::MDRangePolicy<Kokkos::Rank<3>, Kokkos::IndexType<int>>;

Policy3 ownedPolicy(const CellField& field) {
  return Policy3(
      {field.origin(0), field.origin(1), field.origin(2)},
      {field.origin(0) + field.extent(0), field.origin(1) + field.extent(1),
       field.origin(2) + field.extent(2)});
}

Policy3 ownedPolicy(const LocalMesh& mesh) {
  return Policy3(
      {mesh.origin(0), mesh.origin(1), mesh.origin(2)},
      {mesh.origin(0) + mesh.extent(0), mesh.origin(1) + mesh.extent(1),
       mesh.origin(2) + mesh.extent(2)});
}

void fillLinearScalar(CellField& field, const LocalMesh& mesh, const Vec3& a,
                      Real b) {
  const LocalMesh::DeviceData m = mesh.device();
  CellField f = field;
  Kokkos::parallel_for(
      "fv::fill_linear_scalar", ownedPolicy(field),
      KOKKOS_LAMBDA(int i, int j, int k) {
        const Vec3 x = m.cellCentroid(i, j, k);
        f(i, j, k, 0) = a.x * x.x + a.y * x.y + a.z * x.z + b;
      });
}

void fillLinearVector(CellField& field, const LocalMesh& mesh, const Vec3& a,
                      const Vec3& b) {
  const LocalMesh::DeviceData m = mesh.device();
  CellField f = field;
  Kokkos::parallel_for(
      "fv::fill_linear_vector", ownedPolicy(field),
      KOKKOS_LAMBDA(int i, int j, int k) {
        const Vec3 x = m.cellCentroid(i, j, k);
        for (int c = 0; c < 3; ++c)
          f(i, j, k, c) = a[c] * x[c] + b[c];
      });
}

double maxGradientError(const CellField& grad, const LocalMesh& mesh,
                        const Vec3& expected) {
  const LocalMesh::DeviceData m = mesh.device();
  CellField g = grad;
  double local = 0;
  Kokkos::parallel_reduce(
      "fv::gradient_error", ownedPolicy(grad),
      KOKKOS_LAMBDA(int i, int j, int k, double& acc) {
        const Vec3 e = {g(i, j, k, 0) - expected.x, g(i, j, k, 1) - expected.y,
                        g(i, j, k, 2) - expected.z};
        acc = Kokkos::max(acc, Kokkos::sqrt(dot(e, e)));
      },
      Kokkos::Max<double>(local));
  return globalMax(local);
}

double maxDivergenceError(const CellField& div, Real expected) {
  CellField d = div;
  double local = 0;
  Kokkos::parallel_reduce(
      "fv::divergence_error", ownedPolicy(div),
      KOKKOS_LAMBDA(int i, int j, int k, double& acc) {
        acc = Kokkos::max(acc, Kokkos::abs(d(i, j, k, 0) - expected));
      },
      Kokkos::Max<double>(local));
  return globalMax(local);
}

double maxFaceError(const FaceField& face, const LocalMesh& mesh, const Vec3& a,
                    Real b) {
  const LocalMesh::DeviceData m = mesh.device();
  FaceField f = face;
  double local = 0;
  Kokkos::parallel_reduce(
      "fv::face_error", ownedPolicy(mesh),
      KOKKOS_LAMBDA(int i, int j, int k, double& acc) {
        for (int dir = 0; dir < 2 * m.dimension; ++dir) {
          const Vec3 x = m.faceCentroid(i, j, k, dir);
          const Real exact = a.x * x.x + a.y * x.y + a.z * x.z + b;
          acc = Kokkos::max(acc, Kokkos::abs(f(i, j, k, dir, 0) - exact));
        }
      },
      Kokkos::Max<double>(local));
  return globalMax(local);
}

std::vector<BoundaryCondition> dirichletScalar(const LocalMesh& mesh,
                                               const Vec3& a, Real b) {
  BoundaryCondition bc;
  bc.type = BCType::Dirichlet;
  bc.value = [a, b](const Vec3& x, const Vec3&, int) {
    return a.x * x.x + a.y * x.y + a.z * x.z + b;
  };
  return std::vector<BoundaryCondition>(mesh.numPatches(), bc);
}

std::vector<BoundaryCondition> dirichletVector(const LocalMesh& mesh,
                                               const Vec3& a, const Vec3& b) {
  BoundaryCondition bc;
  bc.type = BCType::Dirichlet;
  bc.value = [a, b](const Vec3& x, const Vec3&, int c) {
    return a[c] * x[c] + b[c];
  };
  return std::vector<BoundaryCondition>(mesh.numPatches(), bc);
}

void fillGhosts(CellField& field, const MeshPartition& part,
                const LocalMesh& mesh,
                const std::vector<BoundaryCondition>& conditions) {
  HaloExchange halo(part, field.nGrow(), field.ncomp());
  FieldHalo fieldHalo(halo, field.ncomp());
  fieldHalo.execute(field);
  fillBoundaryGhosts(field, mesh, conditions);
}

std::vector<BoundaryCondition> neumannX(const LocalMesh& mesh) {
  std::vector<BoundaryCondition> bcs(mesh.numPatches(),
                                     constantBoundary(BCType::Neumann, 0.0));
  bcs[mesh.patchIndex("xmin")] = constantBoundary(BCType::Neumann, -1.0);
  bcs[mesh.patchIndex("xmax")] = constantBoundary(BCType::Neumann, 1.0);
  return bcs;
}

double mmsGradientL2(int n) {
  Mesh mesh = test::makeUniformBoxMesh(n, n, n);
  MeshPartition part(n, n, n, true, true, true);
  LocalMesh lm(mesh, part, 1);
  CellField phi(part, 1, 1, "fv::mms_grad_phi");
  CellField grad(part, 1, 3, "fv::mms_grad");
  FaceField face(part, 1, 1, "fv::mms_grad_face");
  const LocalMesh::DeviceData m = lm.device();
  CellField p = phi;
  Kokkos::parallel_for(
      "fv::fill_mms_grad", ownedPolicy(phi),
      KOKKOS_LAMBDA(int i, int j, int k) {
        const Vec3 x = m.cellCentroid(i, j, k);
        p(i, j, k, 0) = Kokkos::sin(2 * pi() * x.x) *
                        Kokkos::sin(2 * pi() * x.y) *
                        Kokkos::sin(2 * pi() * x.z);
      });

  HaloExchange halo(part, 1, 1);
  FieldHalo fieldHalo(halo, 1);
  fieldHalo.execute(phi);
  gradient(phi, face, grad, lm);

  CellField g = grad;
  double local = 0;
  Kokkos::parallel_reduce(
      "fv::mms_grad_l2", ownedPolicy(grad),
      KOKKOS_LAMBDA(int i, int j, int k, double& acc) {
        const Vec3 x = m.cellCentroid(i, j, k);
        const double kk = 2 * pi();
        const double sx = Kokkos::sin(kk * x.x);
        const double sy = Kokkos::sin(kk * x.y);
        const double sz = Kokkos::sin(kk * x.z);
        const double cx = Kokkos::cos(kk * x.x);
        const double cy = Kokkos::cos(kk * x.y);
        const double cz = Kokkos::cos(kk * x.z);
        const Vec3 exact = {kk * cx * sy * sz, kk * sx * cy * sz,
                            kk * sx * sy * cz};
        const Vec3 e = {g(i, j, k, 0) - exact.x, g(i, j, k, 1) - exact.y,
                        g(i, j, k, 2) - exact.z};
        acc += dot(e, e);
      },
      local);

  const double count = static_cast<double>(n) * n * n;
  return std::sqrt(globalSum(local) / count);
}

double mmsDivergenceL2(int n) {
  Mesh mesh = test::makeUniformBoxMesh(n, n, n);
  MeshPartition part(n, n, n, true, true, true);
  LocalMesh lm(mesh, part, 1);
  CellField u(part, 1, 3, "fv::mms_div_u");
  CellField div(part, 1, 1, "fv::mms_div");
  FaceField face(part, 1, 3, "fv::mms_div_face");
  const LocalMesh::DeviceData m = lm.device();
  CellField v = u;
  Kokkos::parallel_for(
      "fv::fill_mms_div", ownedPolicy(u), KOKKOS_LAMBDA(int i, int j, int k) {
        const Vec3 x = m.cellCentroid(i, j, k);
        for (int c = 0; c < 3; ++c)
          v(i, j, k, c) = Kokkos::sin(2 * pi() * x[c]);
      });

  HaloExchange halo(part, 1, 3);
  FieldHalo fieldHalo(halo, 3);
  fieldHalo.execute(u);
  divergence(u, face, div, lm);

  CellField d = div;
  double local = 0;
  Kokkos::parallel_reduce(
      "fv::mms_div_l2", ownedPolicy(div),
      KOKKOS_LAMBDA(int i, int j, int k, double& acc) {
        const Vec3 x = m.cellCentroid(i, j, k);
        const double kk = 2 * pi();
        const double exact =
            kk * (Kokkos::cos(kk * x.x) + Kokkos::cos(kk * x.y) +
                  Kokkos::cos(kk * x.z));
        const double e = d(i, j, k, 0) - exact;
        acc += e * e;
      },
      local);

  const double count = static_cast<double>(n) * n * n;
  return std::sqrt(globalSum(local) / count);
}

double observedOrder(double coarse, double fine) {
  return std::log(coarse / fine) / std::log(2.0);
}

KOKKOS_INLINE_FUNCTION double mms3(double x, double y, double z) {
  return Kokkos::sin(2 * pi() * x) * Kokkos::sin(2 * pi() * y) *
         Kokkos::sin(2 * pi() * z);
}

KOKKOS_INLINE_FUNCTION Vec3 mmsGrad3(double x, double y, double z) {
  const double k = 2 * pi();
  const double sx = Kokkos::sin(k * x), sy = Kokkos::sin(k * y),
               sz = Kokkos::sin(k * z);
  const double cx = Kokkos::cos(k * x), cy = Kokkos::cos(k * y),
               cz = Kokkos::cos(k * z);
  return {k * cx * sy * sz, k * sx * cy * sz, k * sx * sy * cz};
}

double maxAbsCell(const CellField& field) {
  CellField f = field;
  double local = 0;
  Kokkos::parallel_reduce(
      "fv::max_abs_cell", ownedPolicy(field),
      KOKKOS_LAMBDA(int i, int j, int k, double& acc) {
        acc = Kokkos::max(acc, Kokkos::abs(f(i, j, k, 0)));
      },
      Kokkos::Max<double>(local));
  return globalMax(local);
}

void fillConstantVector(CellField& field, const Vec3& value) {
  CellField f = field;
  Kokkos::parallel_for(
      "fv::fill_constant_vector", ownedPolicy(field),
      KOKKOS_LAMBDA(int i, int j, int k) {
        for (int c = 0; c < 3; ++c)
          f(i, j, k, c) = value[c];
      });
}

std::vector<BoundaryCondition> dirichletConstantVector(const LocalMesh& mesh,
                                                       const Vec3& value) {
  BoundaryCondition bc;
  bc.type = BCType::Dirichlet;
  bc.value = [value](const Vec3&, const Vec3&, int c) { return value[c]; };
  return std::vector<BoundaryCondition>(mesh.numPatches(), bc);
}

double mmsConvectionL2(int n, ConvectiveScheme scheme,
                       Limiter limiter = Limiter::Minmod) {
  const int ng = (scheme == ConvectiveScheme::Muscl) ? 2 : 1;
  Mesh mesh = test::makeUniformBoxMesh(n, n, n);
  MeshPartition part(n, n, n, true, true, true);
  LocalMesh lm(mesh, part, ng);
  CellField phi(part, ng, 1, "fv::conv_phi");
  CellField vel(part, ng, 3, "fv::conv_vel");
  CellField div(part, ng, 1, "fv::conv_div");
  FaceField velFaces(part, ng, 3, "fv::conv_vel_face");
  FaceField un(part, ng, 1, "fv::conv_un");
  FaceField flux(part, ng, 1, "fv::conv_flux");
  const LocalMesh::DeviceData m = lm.device();
  const Vec3 a{1, 0.5, 0.25};
  CellField p = phi;
  CellField v = vel;
  Kokkos::parallel_for(
      "fv::fill_conv_phi", ownedPolicy(phi),
      KOKKOS_LAMBDA(int i, int j, int k) {
        const Vec3 x = m.cellCentroid(i, j, k);
        p(i, j, k, 0) = mms3(x.x, x.y, x.z);
      });
  Kokkos::parallel_for(
      "fv::fill_conv_vel", ownedPolicy(vel),
      KOKKOS_LAMBDA(int i, int j, int k) {
        for (int c = 0; c < 3; ++c)
          v(i, j, k, c) = a[c];
      });

  {
    HaloExchange halo(part, ng, 1);
    FieldHalo fieldHalo(halo, 1);
    fieldHalo.execute(phi);
  }
  {
    HaloExchange halo(part, ng, 3);
    FieldHalo fieldHalo(halo, 3);
    fieldHalo.execute(vel);
  }

  faceNormalVelocity(vel, velFaces, un, lm);
  convectiveFlux(un, phi, flux, lm, scheme, limiter);
  divergence(flux, div, lm);

  CellField d = div;
  double local = 0;
  Kokkos::parallel_reduce(
      "fv::conv_mms_l2", ownedPolicy(div),
      KOKKOS_LAMBDA(int i, int j, int k, double& acc) {
        const Vec3 x = m.cellCentroid(i, j, k);
        const double exact = dot(a, mmsGrad3(x.x, x.y, x.z));
        const double e = d(i, j, k, 0) - exact;
        acc += e * e;
      },
      local);
  return std::sqrt(globalSum(local) / (static_cast<double>(n) * n * n));
}

double mmsAdvectionDiffusionL2(int n, ConvectiveScheme scheme) {
  const double nu = 0.1;
  const int ng = 1;
  Mesh mesh = test::makeUniformBoxMesh(n, n, n);
  MeshPartition part(n, n, n, true, true, true);
  LocalMesh lm(mesh, part, ng);
  CellField phi(part, ng, 1, "fv::ad_phi");
  CellField vel(part, ng, 3, "fv::ad_vel");
  CellField div(part, ng, 1, "fv::ad_div");
  CellField lap(part, ng, 1, "fv::ad_lap");
  CellField grad(part, ng, 3, "fv::ad_grad");
  FaceField velFaces(part, ng, 3, "fv::ad_vel_face");
  FaceField un(part, ng, 1, "fv::ad_un");
  FaceField flux(part, ng, 1, "fv::ad_flux");
  FaceField lapFace(part, ng, 1, "fv::ad_lap_face");
  const LocalMesh::DeviceData m = lm.device();
  const Vec3 a{1, 0.5, 0.25};
  CellField p = phi;
  CellField v = vel;
  Kokkos::parallel_for(
      "fv::fill_ad_phi", ownedPolicy(phi), KOKKOS_LAMBDA(int i, int j, int k) {
        const Vec3 x = m.cellCentroid(i, j, k);
        p(i, j, k, 0) = mms3(x.x, x.y, x.z);
      });
  Kokkos::parallel_for(
      "fv::fill_ad_vel", ownedPolicy(vel), KOKKOS_LAMBDA(int i, int j, int k) {
        for (int c = 0; c < 3; ++c)
          v(i, j, k, c) = a[c];
      });

  {
    HaloExchange halo(part, ng, 1);
    FieldHalo fieldHalo(halo, 1);
    fieldHalo.execute(phi);
  }
  {
    HaloExchange halo(part, ng, 3);
    FieldHalo fieldHalo(halo, 3);
    fieldHalo.execute(vel);
  }

  faceNormalVelocity(vel, velFaces, un, lm);
  convectiveFlux(un, phi, flux, lm, scheme);
  divergence(flux, div, lm);
  laplacian(phi, lapFace, grad, lap, lm);

  CellField d = div;
  CellField l = lap;
  double local = 0;
  Kokkos::parallel_reduce(
      "fv::ad_mms_l2", ownedPolicy(div),
      KOKKOS_LAMBDA(int i, int j, int k, double& acc) {
        const Vec3 x = m.cellCentroid(i, j, k);
        const double kk = 2 * pi();
        const double conv = dot(a, mmsGrad3(x.x, x.y, x.z));
        const double lapExact = -3 * kk * kk * mms3(x.x, x.y, x.z);
        const double source = conv - nu * lapExact;
        const double residual = d(i, j, k, 0) - nu * l(i, j, k, 0) - source;
        acc += residual * residual;
      },
      local);
  return std::sqrt(globalSum(local) / (static_cast<double>(n) * n * n));
}

double globalMin(double value) {
#ifdef SATURN_USE_MPI
  double out = 0;
  MPI_Allreduce(&value, &out, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
  return out;
#else
  return value;
#endif
}

double cellMin(const CellField& field) {
  CellField f = field;
  double local = std::numeric_limits<double>::max();
  Kokkos::parallel_reduce(
      "fv::cell_min", ownedPolicy(field),
      KOKKOS_LAMBDA(int i, int j, int k, double& acc) {
        acc = Kokkos::min(acc, f(i, j, k, 0));
      },
      Kokkos::Min<double>(local));
  return globalMin(local);
}

double cellMax(const CellField& field) {
  CellField f = field;
  double local = -std::numeric_limits<double>::max();
  Kokkos::parallel_reduce(
      "fv::cell_max", ownedPolicy(field),
      KOKKOS_LAMBDA(int i, int j, int k, double& acc) {
        acc = Kokkos::max(acc, f(i, j, k, 0));
      },
      Kokkos::Max<double>(local));
  return globalMax(local);
}

double maxAbsFace(const FaceField& field) {
  FaceField f = field;
  double local = 0;
  const int dim = 3;
  Kokkos::parallel_reduce(
      "fv::max_abs_face",
      Kokkos::MDRangePolicy<Kokkos::Rank<3>, Kokkos::IndexType<int>>(
          {field.origin(0), field.origin(1), field.origin(2)},
          {field.origin(0) + field.extent(0), field.origin(1) + field.extent(1),
           field.origin(2) + field.extent(2)}),
      KOKKOS_LAMBDA(int i, int j, int k, double& acc) {
        for (int dir = 0; dir < 2 * dim; ++dir)
          acc = Kokkos::max(acc, Kokkos::abs(f(i, j, k, dir, 0)));
      },
      Kokkos::Max<double>(local));
  return globalMax(local);
}

double maxDiffFace(const FaceField& a, const FaceField& b) {
  FaceField fa = a;
  FaceField fb = b;
  double local = 0;
  const int dim = 3;
  Kokkos::parallel_reduce(
      "fv::max_diff_face",
      Kokkos::MDRangePolicy<Kokkos::Rank<3>, Kokkos::IndexType<int>>(
          {a.origin(0), a.origin(1), a.origin(2)},
          {a.origin(0) + a.extent(0), a.origin(1) + a.extent(1),
           a.origin(2) + a.extent(2)}),
      KOKKOS_LAMBDA(int i, int j, int k, double& acc) {
        for (int dir = 0; dir < 2 * dim; ++dir)
          acc = Kokkos::max(
              acc, Kokkos::abs(fa(i, j, k, dir, 0) - fb(i, j, k, dir, 0)));
      },
      Kokkos::Max<double>(local));
  return globalMax(local);
}

void fillQuadraticVector(CellField& field, const LocalMesh& mesh) {
  const LocalMesh::DeviceData m = mesh.device();
  CellField f = field;
  Kokkos::parallel_for(
      "fv::fill_quadratic_vector", ownedPolicy(field),
      KOKKOS_LAMBDA(int i, int j, int k) {
        const Vec3 x = m.cellCentroid(i, j, k);
        for (int c = 0; c < 3; ++c)
          f(i, j, k, c) = x[c] * x[c];
      });
}

std::vector<BoundaryCondition> dirichletQuadraticVector(const LocalMesh& mesh) {
  BoundaryCondition bc;
  bc.type = BCType::Dirichlet;
  bc.value = [](const Vec3& x, const Vec3&, int c) { return x[c] * x[c]; };
  return std::vector<BoundaryCondition>(mesh.numPatches(), bc);
}

}  // namespace

TEST_CASE("fv: LocalMesh extracts uniform hex geometry") {
  const int n = 4;
  Mesh mesh = test::makeUniformBoxMesh(n, n, n);
  MeshPartition part(n, n, n, false, false, false);
  LocalMesh lm(mesh, part, 2);
  CHECK(lm.dimension() == 3);
  CHECK(lm.numPatches() == 6);
  CHECK(lm.patchIndex("xmin") >= 0);
  CHECK(lm.patchIndex("zmax") >= 0);

  const Real h = Real(1) / n;
  double localVolume = 0;
  for (int k = lm.origin(2); k < lm.origin(2) + lm.extent(2); ++k) {
    for (int j = lm.origin(1); j < lm.origin(1) + lm.extent(1); ++j) {
      for (int i = lm.origin(0); i < lm.origin(0) + lm.extent(0); ++i) {
        const Real volume = lm.cellVolume(i, j, k);
        CHECK(volume == doctest::Approx(h * h * h));
        localVolume += volume;

        const Vec3 x = lm.cellCentroid(i, j, k);
        CHECK(x.x == doctest::Approx((i + Real(0.5)) * h));
        CHECK(x.y == doctest::Approx((j + Real(0.5)) * h));
        CHECK(x.z == doctest::Approx((k + Real(0.5)) * h));

        for (int dir = 0; dir < 6; ++dir) {
          CHECK(lm.faceArea(i, j, k, dir) == doctest::Approx(h * h));
          const Vec3 normal = lm.faceNormal(i, j, k, dir);
          CHECK(dot(normal, normal) == doctest::Approx(1.0));
        }
      }
    }
  }
  CHECK(globalSum(localVolume) == doctest::Approx(1.0));

  if (lm.origin(0) == 0) {
    const int i = 0;
    const int j = lm.origin(1);
    const int k = lm.origin(2);
    CHECK(lm.facePatch(i, j, k, 0) == lm.patchIndex("xmin"));
    const Vec3 normal = lm.faceNormal(i, j, k, 0);
    CHECK(normal.x == doctest::Approx(-1.0));
    CHECK(std::abs(normal.y) < 1e-12);
    CHECK(std::abs(normal.z) < 1e-12);
    CHECK(lm.faceDistance(i, j, k, 0) == doctest::Approx(Real(0.5) * h));
  }
}

TEST_CASE("fv: interpolation of a linear field is exact") {
  const int n = 6;
  Mesh mesh = test::makeUniformBoxMesh(n, n, n);
  MeshPartition part(n, n, n, false, false, false);
  LocalMesh lm(mesh, part, 1);
  CellField phi(part, 1, 1, "fv::linear_phi");
  FaceField face(part, 1, 1, "fv::linear_face");
  const Vec3 a{2, -3, 0.5};
  const Real b = 1;

  fillLinearScalar(phi, lm, a, b);
  fillGhosts(phi, part, lm, dirichletScalar(lm, a, b));
  interpolate(phi, face, lm);
  CHECK(maxFaceError(face, lm, a, b) < 1e-12);
}

TEST_CASE("fv: gradient of a linear field is exact with Dirichlet ghosts") {
  const int n = 6;
  Mesh mesh = test::makeUniformBoxMesh(n, n, n);
  MeshPartition part(n, n, n, false, false, false);
  LocalMesh lm(mesh, part, 1);
  CellField phi(part, 1, 1, "fv::grad_phi");
  CellField grad(part, 1, 3, "fv::grad");
  FaceField face(part, 1, 1, "fv::grad_face");
  const Vec3 a{2, -3, 0.5};
  const Real b = 1;

  fillLinearScalar(phi, lm, a, b);
  fillGhosts(phi, part, lm, dirichletScalar(lm, a, b));
  gradient(phi, face, grad, lm);
  CHECK(maxGradientError(grad, lm, a) < 1e-12);

  if (lm.origin(0) == 0) {
    const int i = 0;
    const int j = lm.origin(1);
    const int k = lm.origin(2);
    const Vec3 x = lm.cellCentroid(i, j, k);
    const Real exact = a.x * (x.x - Real(1) / n) + a.y * x.y + a.z * x.z + b;
    CHECK(std::abs(phi(i - 1, j, k, 0) - exact) < 1e-12);
  }
}

TEST_CASE("fv: 2D LocalMesh gradient ignores the empty direction") {
  const int n = 8;
  Mesh mesh = test::makeUniformBoxMesh(n, n, 1);
  MeshPartition part(n, n, 1, false, false, false);
  LocalMesh lm(mesh, part, 1);
  CHECK(lm.dimension() == 2);
  CellField phi(part, 1, 1, "fv::grad2d_phi");
  CellField grad(part, 1, 3, "fv::grad2d");
  FaceField face(part, 1, 1, "fv::grad2d_face");
  const Vec3 a{2, -3, 0};
  const Real b = 1;

  fillLinearScalar(phi, lm, a, b);
  fillGhosts(phi, part, lm, dirichletScalar(lm, a, b));
  gradient(phi, face, grad, lm);
  CHECK(maxGradientError(grad, lm, a) < 1e-12);
}

TEST_CASE("fv: divergence of a linear field is exact") {
  const int n = 6;
  Mesh mesh = test::makeUniformBoxMesh(n, n, n);
  MeshPartition part(n, n, n, false, false, false);
  LocalMesh lm(mesh, part, 1);
  CellField u(part, 1, 3, "fv::linear_u");
  CellField div(part, 1, 1, "fv::linear_div");
  FaceField face(part, 1, 3, "fv::linear_div_face");
  const Vec3 a{1, 2, 3};
  const Vec3 b{0, 0, 0};

  fillLinearVector(u, lm, a, b);
  fillGhosts(u, part, lm, dirichletVector(lm, a, b));
  divergence(u, face, div, lm);
  CHECK(maxDivergenceError(div, a.x + a.y + a.z) < 1e-12);
}

TEST_CASE("fv: Neumann ghosts give the exact linear gradient") {
  const int n = 6;
  Mesh mesh = test::makeUniformBoxMesh(n, n, n);
  MeshPartition part(n, n, n, false, false, false);
  LocalMesh lm(mesh, part, 1);
  CellField phi(part, 1, 1, "fv::neumann_phi");
  CellField grad(part, 1, 3, "fv::neumann_grad");
  FaceField face(part, 1, 1, "fv::neumann_face");
  const Vec3 a{1, 0, 0};

  fillLinearScalar(phi, lm, a, 0);
  fillGhosts(phi, part, lm, neumannX(lm));
  gradient(phi, face, grad, lm);
  CHECK(maxGradientError(grad, lm, a) < 1e-12);
}

TEST_CASE("fv: periodic divergence integrates to zero") {
  const int n = 16;
  Mesh mesh = test::makeUniformBoxMesh(n, n, n);
  MeshPartition part(n, n, n, true, true, true);
  LocalMesh lm(mesh, part, 1);
  CellField u(part, 1, 3, "fv::div_free_u");
  CellField div(part, 1, 1, "fv::div_free");
  FaceField face(part, 1, 3, "fv::div_free_face");
  const LocalMesh::DeviceData m = lm.device();
  CellField v = u;
  Kokkos::parallel_for(
      "fv::fill_div_free", ownedPolicy(u), KOKKOS_LAMBDA(int i, int j, int k) {
        const Vec3 x = m.cellCentroid(i, j, k);
        for (int c = 0; c < 3; ++c)
          v(i, j, k, c) = Kokkos::sin(2 * pi() * x[c]);
      });

  HaloExchange halo(part, 1, 3);
  FieldHalo fieldHalo(halo, 3);
  fieldHalo.execute(u);
  divergence(u, face, div, lm);

  CellField d = div;
  double local = 0;
  Kokkos::parallel_reduce(
      "fv::div_integral", ownedPolicy(div),
      KOKKOS_LAMBDA(int i, int j, int k, double& acc) {
        acc += d(i, j, k, 0) * m.cellVolume(i, j, k);
      },
      local);
  CHECK(std::abs(globalSum(local)) < 1e-12);
}

TEST_CASE("fv: gradient MMS on LocalMesh is second order") {
  const double e0 = mmsGradientL2(8);
  const double e1 = mmsGradientL2(16);
  const double e2 = mmsGradientL2(32);
  const double p1 = observedOrder(e0, e1);
  const double p2 = observedOrder(e1, e2);
  MESSAGE("LocalMesh gradient MMS L2: " << std::setprecision(17) << e0 << " "
                                        << e1 << " " << e2);
  MESSAGE("LocalMesh gradient orders: " << p1 << " " << p2);
  CHECK(e2 < e1);
  CHECK(e1 < e0);
  CHECK(std::abs(p1 - 2.0) < 0.2);
  CHECK(std::abs(p2 - 2.0) < 0.2);
}

TEST_CASE("fv: divergence MMS on LocalMesh is second order") {
  const double e0 = mmsDivergenceL2(8);
  const double e1 = mmsDivergenceL2(16);
  const double e2 = mmsDivergenceL2(32);
  const double p1 = observedOrder(e0, e1);
  const double p2 = observedOrder(e1, e2);
  MESSAGE("LocalMesh divergence MMS L2: " << std::setprecision(17) << e0 << " "
                                          << e1 << " " << e2);
  MESSAGE("LocalMesh divergence orders: " << p1 << " " << p2);
  CHECK(e2 < e1);
  CHECK(e1 < e0);
  CHECK(std::abs(p1 - 2.0) < 0.2);
  CHECK(std::abs(p2 - 2.0) < 0.2);
}

TEST_CASE("fv: non-orthogonal Laplacian is exact for a linear field") {
  const int n = 6;
  Mesh mesh = test::makeShearedBoxMesh(n, n, n, 0.5);
  MeshPartition part(n, n, n, false, false, false);
  LocalMesh lm(mesh, part, 1);
  CellField phi(part, 1, 1, "fv::skew_phi");
  CellField grad(part, 1, 3, "fv::skew_grad");
  CellField lap(part, 1, 1, "fv::skew_lap");
  FaceField face(part, 1, 1, "fv::skew_face");
  const Vec3 a{2, -3, 0.5};
  const Real b = 1;

  fillLinearScalar(phi, lm, a, b);
  fillGhosts(phi, part, lm, dirichletScalar(lm, a, b));
  laplacian(phi, face, grad, lap, lm);
  const double error = maxAbsCell(lap);
  MESSAGE("sheared-mesh linear Laplacian max |L phi| = " << error);
  CHECK(error < 1e-11);
}

TEST_CASE("fv: constant-field convection has zero divergence") {
  const int n = 8;
  Mesh mesh = test::makeUniformBoxMesh(n, n, n);
  MeshPartition part(n, n, n, false, false, false);
  LocalMesh lm(mesh, part, 1);
  CellField phi(part, 1, 1, "fv::const_phi");
  CellField vel(part, 1, 3, "fv::const_vel");
  CellField div(part, 1, 1, "fv::const_div");
  FaceField velFaces(part, 1, 3, "fv::const_vel_face");
  FaceField un(part, 1, 1, "fv::const_un");
  FaceField flux(part, 1, 1, "fv::const_flux");
  const Vec3 a{1, 0.5, 0.25};

  phi.fill(1.0);
  fillConstantVector(vel, a);
  fillGhosts(phi, part, lm, dirichletScalar(lm, {0, 0, 0}, 1.0));
  fillGhosts(vel, part, lm, dirichletConstantVector(lm, a));
  faceNormalVelocity(vel, velFaces, un, lm);

  for (ConvectiveScheme scheme :
       {ConvectiveScheme::Upwind, ConvectiveScheme::Rusanov}) {
    convectiveFlux(un, phi, flux, lm, scheme);
    divergence(flux, div, lm);
    CHECK(maxAbsCell(div) < 1e-12);
  }
}

TEST_CASE("fv: upwind convection MMS is first order") {
  const double e0 = mmsConvectionL2(8, ConvectiveScheme::Upwind);
  const double e1 = mmsConvectionL2(16, ConvectiveScheme::Upwind);
  const double e2 = mmsConvectionL2(32, ConvectiveScheme::Upwind);
  const double p1 = observedOrder(e0, e1);
  const double p2 = observedOrder(e1, e2);
  MESSAGE("upwind convection MMS L2: " << std::setprecision(17) << e0 << " "
                                       << e1 << " " << e2);
  MESSAGE("upwind convection orders: " << p1 << " " << p2);
  CHECK(e2 < e1);
  CHECK(e1 < e0);
  CHECK(std::abs(p1 - 1.0) < 0.3);
  CHECK(std::abs(p2 - 1.0) < 0.3);
}

TEST_CASE("fv: Rusanov reduces to upwind for scalar advection") {
  // For a scalar with one wave speed, the Rusanov flux is algebraically the
  // upwind flux. Keep the branch tested; the values may differ only by
  // roundoff from the different expression order.
  for (int n : {8, 16, 32}) {
    const double upwind = mmsConvectionL2(n, ConvectiveScheme::Upwind);
    const double rusanov = mmsConvectionL2(n, ConvectiveScheme::Rusanov);
    CHECK(std::abs(upwind - rusanov) < 1e-12);
  }
}

TEST_CASE("fv: steady advection-diffusion MMS is first order") {
  const double e0 = mmsAdvectionDiffusionL2(8, ConvectiveScheme::Upwind);
  const double e1 = mmsAdvectionDiffusionL2(16, ConvectiveScheme::Upwind);
  const double e2 = mmsAdvectionDiffusionL2(32, ConvectiveScheme::Upwind);
  const double p1 = observedOrder(e0, e1);
  const double p2 = observedOrder(e1, e2);
  MESSAGE("advection-diffusion MMS L2: " << std::setprecision(17) << e0 << " "
                                         << e1 << " " << e2);
  MESSAGE("advection-diffusion orders: " << p1 << " " << p2);
  CHECK(e2 < e1);
  CHECK(e1 < e0);
  CHECK(std::abs(p1 - 1.0) < 0.3);
  CHECK(std::abs(p2 - 1.0) < 0.3);
}

TEST_CASE("fv: MUSCL preserves a linear field in the interior") {
  const int n = 8;
  const int ng = 2;
  Mesh mesh = test::makeUniformBoxMesh(n, n, n);
  MeshPartition part(n, n, n, false, false, false);
  LocalMesh lm(mesh, part, ng);
  CellField phi(part, ng, 1, "fv::muscl_linear_phi");
  CellField vel(part, ng, 3, "fv::muscl_linear_vel");
  CellField div(part, ng, 1, "fv::muscl_linear_div");
  FaceField velFaces(part, ng, 3, "fv::muscl_linear_vel_face");
  FaceField un(part, ng, 1, "fv::muscl_linear_un");
  FaceField flux(part, ng, 1, "fv::muscl_linear_flux");
  const Vec3 aPhi{2, -3, 0.5};
  const Vec3 aVel{1, 0.5, 0.25};
  const Real b = 1;

  fillLinearScalar(phi, lm, aPhi, b);
  fillConstantVector(vel, aVel);
  fillGhosts(phi, part, lm, dirichletScalar(lm, aPhi, b));
  fillGhosts(vel, part, lm, dirichletConstantVector(lm, aVel));
  faceNormalVelocity(vel, velFaces, un, lm);

  const int gx = part.globalExtent(0);
  const int gy = part.globalExtent(1);
  const int gz = part.globalExtent(2);
  const double exact = aVel.x * aPhi.x + aVel.y * aPhi.y + aVel.z * aPhi.z;
  CellField d = div;

  for (Limiter limiter :
       {Limiter::Minmod, Limiter::VanLeer, Limiter::Superbee}) {
    convectiveFlux(un, phi, flux, lm, ConvectiveScheme::Muscl, limiter);
    divergence(flux, div, lm);
    double local = 0;
    Kokkos::parallel_reduce(
        "fv::muscl_linear_error", ownedPolicy(div),
        KOKKOS_LAMBDA(int i, int j, int k, double& acc) {
          if (i <= 1 || i >= gx - 2 || j <= 1 || j >= gy - 2 || k <= 1 ||
              k >= gz - 2)
            return;
          acc = Kokkos::max(acc, Kokkos::abs(d(i, j, k, 0) - exact));
        },
        Kokkos::Max<double>(local));
    CHECK(globalMax(local) < 1e-11);
  }
}

TEST_CASE("fv: MUSCL convection MMS improves on first order") {
  for (Limiter limiter :
       {Limiter::Minmod, Limiter::VanLeer, Limiter::Superbee}) {
    const double e0 = mmsConvectionL2(8, ConvectiveScheme::Muscl, limiter);
    const double e1 = mmsConvectionL2(16, ConvectiveScheme::Muscl, limiter);
    const double e2 = mmsConvectionL2(32, ConvectiveScheme::Muscl, limiter);
    const double p1 = observedOrder(e0, e1);
    const double p2 = observedOrder(e1, e2);
    MESSAGE("MUSCL convection L2: " << std::setprecision(17) << e0 << " " << e1
                                    << " " << e2);
    MESSAGE("MUSCL convection orders: " << p1 << " " << p2);
    CHECK(e2 < e1);
    CHECK(e1 < e0);
    // Superbee is the most compressive limiter and clips smooth extrema
    // harder; minmod/van Leer stay closer to second order.
    const double threshold = (limiter == Limiter::Superbee) ? 1.0 : 1.4;
    CHECK(p1 > threshold);
    CHECK(p2 > threshold);
  }
}

TEST_CASE("fv: MUSCL keeps a periodic step bounded") {
  const int n = 64;
  const int ng = 2;
  Mesh mesh = test::makeUniformBoxMesh(n, 1, 1);
  MeshPartition part(n, 1, 1, true, false, false);
  LocalMesh lm(mesh, part, ng);
  CellField phi(part, ng, 1, "fv::tvd_phi");
  CellField vel(part, ng, 3, "fv::tvd_vel");
  CellField div(part, ng, 1, "fv::tvd_div");
  FaceField velFaces(part, ng, 3, "fv::tvd_vel_face");
  FaceField un(part, ng, 1, "fv::tvd_un");
  FaceField flux(part, ng, 1, "fv::tvd_flux");
  const LocalMesh::DeviceData m = lm.device();

  fillConstantVector(vel, Vec3{1, 0, 0});
  HaloExchange haloVel(part, ng, 3);
  FieldHalo fieldHaloVel(haloVel, 3);
  fieldHaloVel.execute(vel);
  faceNormalVelocity(vel, velFaces, un, lm);

  HaloExchange haloPhi(part, ng, 1);
  FieldHalo fieldHaloPhi(haloPhi, 1);
  const double h = 1.0 / n;
  const double dt = 0.2 * h;
  const int steps = 100;
  CellField p = phi;
  CellField d = div;

  for (Limiter limiter :
       {Limiter::Minmod, Limiter::VanLeer, Limiter::Superbee}) {
    Kokkos::parallel_for(
        "fv::tvd_init", ownedPolicy(phi), KOKKOS_LAMBDA(int i, int j, int k) {
          const Vec3 x = m.cellCentroid(i, j, k);
          p(i, j, k, 0) = (x.x < 0.5) ? Real(1) : Real(0);
        });
    fieldHaloPhi.execute(phi);

    for (int step = 0; step < steps; ++step) {
      convectiveFlux(un, phi, flux, lm, ConvectiveScheme::Muscl, limiter);
      divergence(flux, div, lm);
      Kokkos::parallel_for(
          "fv::tvd_update", ownedPolicy(phi),
          KOKKOS_LAMBDA(int i, int j, int k) {
            p(i, j, k, 0) -= dt * d(i, j, k, 0);
          });
      fieldHaloPhi.execute(phi);
    }

    const double lo = cellMin(phi);
    const double hi = cellMax(phi);
    MESSAGE("TVD step range: " << lo << " " << hi);
    CHECK(lo >= -1e-12);
    CHECK(hi <= 1 + 1e-12);
  }
}

TEST_CASE("fv: discrete divergence theorem on a sheared mesh") {
  const int n = 6;
  Mesh mesh = test::makeShearedBoxMesh(n, n, n, 0.5);
  MeshPartition part(n, n, n, false, false, false);
  LocalMesh lm(mesh, part, 1);
  CellField vel(part, 1, 3, "fv::divthm_vel");
  CellField div(part, 1, 1, "fv::divthm_div");
  FaceField velFaces(part, 1, 3, "fv::divthm_face");
  const LocalMesh::DeviceData m = lm.device();

  fillQuadraticVector(vel, lm);
  fillGhosts(vel, part, lm, dirichletQuadraticVector(lm));
  interpolate(vel, velFaces, lm);
  divergence(velFaces, div, lm);

  CellField d = div;
  double localDiv = 0;
  Kokkos::parallel_reduce(
      "fv::divthm_lhs", ownedPolicy(div),
      KOKKOS_LAMBDA(int i, int j, int k, double& acc) {
        acc += d(i, j, k, 0) * m.cellVolume(i, j, k);
      },
      localDiv);

  FaceField f = velFaces;
  double localFlux = 0;
  Kokkos::parallel_reduce(
      "fv::divthm_rhs", ownedPolicy(vel),
      KOKKOS_LAMBDA(int i, int j, int k, double& acc) {
        for (int dir = 0; dir < 2 * m.dimension; ++dir) {
          if (m.boundaryPatch(i, j, k, dir) < 0 || m.periodic[dir / 2])
            continue;
          const Vec3 n = m.faceNormal(i, j, k, dir);
          const Vec3 u = {f(i, j, k, dir, 0), f(i, j, k, dir, 1),
                          f(i, j, k, dir, 2)};
          acc += m.faceArea(i, j, k, dir) * dot(u, n);
        }
      },
      localFlux);

  const double lhs = globalSum(localDiv);
  const double rhs = globalSum(localFlux);
  MESSAGE("divergence theorem: " << std::setprecision(17) << lhs << " " << rhs);
  CHECK(std::abs(lhs - rhs) < 1e-11 * std::max(1.0, std::abs(rhs)));
}

TEST_CASE("fv: Rhie-Chow vanishes for a linear pressure field") {
  const int n = 8;
  Mesh mesh = test::makeUniformBoxMesh(n, n, n);
  MeshPartition part(n, n, n, false, false, false);
  LocalMesh lm(mesh, part, 1);
  CellField vel(part, 1, 3, "fv::rc_vel");
  CellField pressure(part, 1, 1, "fv::rc_p");
  CellField gradP(part, 1, 3, "fv::rc_grad");
  FaceField velFaces(part, 1, 3, "fv::rc_vel_face");
  FaceField pFaces(part, 1, 1, "fv::rc_p_face");
  FaceField un(part, 1, 1, "fv::rc_un");
  FaceField plain(part, 1, 1, "fv::rc_plain");
  FaceField plainFaces(part, 1, 3, "fv::rc_plain_faces");
  const Vec3 aVel{1, 0.5, 0.25};
  const Vec3 aP{2, -3, 0.5};

  fillConstantVector(vel, aVel);
  fillLinearScalar(pressure, lm, aP, 1);
  fillGhosts(vel, part, lm, dirichletConstantVector(lm, aVel));
  fillGhosts(pressure, part, lm, dirichletScalar(lm, aP, 1));
  faceNormalVelocity(vel, plainFaces, plain, lm);
  rhieChowFlux(vel, pressure, velFaces, pFaces, gradP, un, lm, 1.0);
  CHECK(maxDiffFace(un, plain) < 1e-12);
}

TEST_CASE("fv: Rhie-Chow suppresses a checkerboard pressure") {
  const int n = 8;
  Mesh mesh = test::makeUniformBoxMesh(n, n, n);
  MeshPartition part(n, n, n, true, true, true);
  LocalMesh lm(mesh, part, 1);
  CellField vel(part, 1, 3, "fv::rc_cb_vel");
  CellField pressure(part, 1, 1, "fv::rc_cb_p");
  CellField gradP(part, 1, 3, "fv::rc_cb_grad");
  FaceField velFaces(part, 1, 3, "fv::rc_cb_vel_face");
  FaceField pFaces(part, 1, 1, "fv::rc_cb_p_face");
  FaceField un(part, 1, 1, "fv::rc_cb_un");
  const LocalMesh::DeviceData m = lm.device();

  vel.fill(0.0);
  CellField p = pressure;
  Kokkos::parallel_for(
      "fv::rc_cb_fill", ownedPolicy(pressure),
      KOKKOS_LAMBDA(int i, int j, int k) {
        p(i, j, k, 0) = ((i + j + k) % 2 == 0) ? Real(1) : Real(-1);
      });
  HaloExchange haloP(part, 1, 1);
  FieldHalo fieldHaloP(haloP, 1);
  fieldHaloP.execute(pressure);

  rhieChowFlux(vel, pressure, velFaces, pFaces, gradP, un, lm, 1.0);
  CHECK(maxAbsFace(un) > 0.1);
}

TEST_CASE("fv: Burgers constant flux has zero divergence") {
  const int n = 32;
  Mesh mesh = test::makeUniformBoxMesh(n, 1, 1, {-1, 0, 0}, {1, 1, 1});
  MeshPartition part(n, 1, 1, true, false, false);
  LocalMesh lm(mesh, part, 2);
  CellField u(part, 2, 1, "dbg_burgers_u");
  u.fill(1.0);
  HaloExchange halo(part, 2, 1);
  FieldHalo fieldHalo(halo, 1);
  fieldHalo.execute(u);
  FaceField flux(part, 2, 1, "dbg_burgers_flux");
  burgersFlux(u, flux, lm, ConvectiveScheme::Muscl, Limiter::VanLeer);
  CellField div(part, 2, 1, "dbg_burgers_div");
  divergence(flux, div, lm);
  CHECK(maxAbsCell(div) < 1e-12);
  if (lm.origin(0) == 0) {
    CHECK(flux(0, 0, 0, 0, 0) == doctest::Approx(-0.5).epsilon(1e-12));
    CHECK(flux(0, 0, 0, 1, 0) == doctest::Approx(0.5).epsilon(1e-12));
    CHECK(div(0, 0, 0, 0) == doctest::Approx(0.0).epsilon(1e-12));
  }
}
