#include "fv/BoundaryFill.hpp"

#include <stdexcept>
#include <string>

namespace saturn {

void fillBoundaryGhosts(CellField& field, const LocalMesh& mesh,
                        const std::vector<BoundaryCondition>& conditions) {
  if (static_cast<int>(conditions.size()) != mesh.numPatches())
    throw std::invalid_argument(
        "fillBoundaryGhosts: one condition is required per patch");
  if (field.nGrow() != mesh.nGrow())
    throw std::invalid_argument(
        "fillBoundaryGhosts: field and LocalMesh ghost widths differ");
  for (int d = 0; d < 3; ++d)
    if (field.origin(d) != mesh.origin(d) || field.extent(d) != mesh.extent(d))
      throw std::invalid_argument(
          "fillBoundaryGhosts: field layout does not match LocalMesh");

  const int ng = field.nGrow();
  const int nc = field.ncomp();
  const int i0 = mesh.origin(0);
  const int j0 = mesh.origin(1);
  const int k0 = mesh.origin(2);
  const int nx = mesh.extent(0);
  const int ny = mesh.extent(1);
  const int nz = mesh.extent(2);
  auto host = field.hostView();

  auto at = [&](int i, int j, int k, int c) -> Real& {
    const int ii = i - (i0 - ng);
    const int jj = j - (j0 - ng);
    const int kk = k - (k0 - ng);
    return host(ii, jj, kk, c);
  };

  for (int k = k0; k < k0 + nz; ++k) {
    for (int j = j0; j < j0 + ny; ++j) {
      for (int i = i0; i < i0 + nx; ++i) {
        for (int dir = 0; dir < 2 * mesh.dimension(); ++dir) {
          const int patch = mesh.facePatch(i, j, k, dir);
          if (patch < 0)
            continue;
          const int axis = dir / 2;
          if (mesh.periodic(axis))
            continue;
          const BoundaryCondition& bc = conditions[patch];
          if (!bc.value)
            throw std::invalid_argument("fillBoundaryGhosts: patch '" +
                                        mesh.patchName(patch) +
                                        "' has no callback");

          const Vec3 xf = mesh.faceCentroid(i, j, k, dir);
          const Vec3 normal = mesh.faceNormal(i, j, k, dir);
          const Real distance = mesh.faceDistance(i, j, k, dir);
          int di = 0, dj = 0, dk = 0;
          faceDirectionOffset(dir, di, dj, dk);

          for (int layer = 1; layer <= ng; ++layer) {
            const int gi = i + layer * di;
            const int gj = j + layer * dj;
            const int gk = k + layer * dk;
            for (int c = 0; c < nc; ++c) {
              const Real owner = at(i, j, k, c);
              const Real target = bc.value(xf, normal, c);
              at(gi, gj, gk, c) =
                  (bc.type == BCType::Dirichlet)
                      ? Real(2) * target - owner
                      : owner + Real(2 * layer) * distance * target;
            }
          }
        }
      }
    }
  }

  field.syncFromHost(host);
}

}  // namespace saturn
