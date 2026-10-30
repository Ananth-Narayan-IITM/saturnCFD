#include <doctest/doctest.h>

#include <cmath>
#include <iomanip>
#include <vector>

#include <Kokkos_Core.hpp>

#ifdef SATURN_USE_MPI
#include <mpi.h>
#endif

#include "comm/HaloExchange.hpp"
#include "comm/Partition.hpp"
#include "fv/BoundaryFill.hpp"
#include "fv/CellField.hpp"
#include "fv/FaceField.hpp"
#include "fv/FieldHalo.hpp"
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
