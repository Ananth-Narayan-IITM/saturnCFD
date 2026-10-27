#include "io/VTKWriter.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <stdexcept>

namespace saturn {
namespace {

std::string stepTag(int step) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%06d", step);
  return buf;
}

}  // namespace

VTKWriter::VTKWriter(std::string base) : mBase(std::move(base)) {
  const auto slash = mBase.find_last_of('/');
  if (slash != std::string::npos) {
    std::error_code ec;
    std::filesystem::create_directories(mBase.substr(0, slash), ec);
  }
}

void VTKWriter::write(const MultiFab& mf, const Geometry& geom, int step,
                      double time, const std::vector<std::string>& compNames) {
  const std::string tag = stepTag(step);
  const int ng = mf.nGrow();
  const int nc = mf.numComp();

  // The field may live in GPU memory. Copy to the host ONCE for the whole
  // MultiFab, then index the flat layout directly. (Reading through an Array4
  // on the host would dereference device memory and segfault.)
  auto host =
      Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), mf.data());

  // Walk the local boxes in the same order MultiFab::allocate() laid them out,
  // accumulating each patch's offset into the flat buffer.
  std::size_t base = 0;

  for (int lb = 0; lb < mf.numLocalBoxes(); ++lb) {
    const int gbox = mf.localToGlobal(lb);
    const Box bx = mf.boxArray()[gbox];

    const int nx = bx.length(0);
    const int ny = bx.length(1);
    const int nz = (NDIM == 3) ? bx.length(2) : 1;

    const int ex = nx + 2 * ng;  // extent including ghosts
    const int ey = ny + 2 * ng;
    const int ez = (NDIM == 3) ? (nz + 2 * ng) : 1;

    const std::size_t sy = static_cast<std::size_t>(ex);
    const std::size_t sz = sy * static_cast<std::size_t>(ey);
    const std::size_t sc = sz * static_cast<std::size_t>(ez);
    const std::size_t patchSize = sc * static_cast<std::size_t>(nc);

    // (i, j, k) are global indices; convert to the ghost-inclusive local slot.
    auto at = [&](int c, int i, int j, int k) -> Real {
      const std::size_t ii = static_cast<std::size_t>(i - bx.lo[0] + ng);
      const std::size_t jj = static_cast<std::size_t>(j - bx.lo[1] + ng);
      const std::size_t kk =
          (NDIM == 3) ? static_cast<std::size_t>(k - bx.lo[2] + ng) : 0u;
      return host[base + static_cast<std::size_t>(c) * sc + kk * sz + jj * sy +
                  ii];
    };

    char name[512];
    std::snprintf(name, sizeof(name), "%s_%s_p%04d.vti", mBase.c_str(),
                  tag.c_str(), gbox);

    std::ofstream os(name);
    if (!os)
      throw std::runtime_error(std::string("VTKWriter: cannot open ") + name);

    Real org[3] = {0, 0, 0};
    Real spc[3] = {geom.dx, geom.dx, static_cast<Real>(1)};
    for (int d = 0; d < NDIM; ++d)
      org[d] = geom.origin[d] +
               (static_cast<Real>(bx.lo[d] - geom.domain.lo[d])) * geom.dx;

    os << "<?xml version=\"1.0\"?>\n";
    os << "<VTKFile type=\"ImageData\" version=\"0.1\" "
          "byte_order=\"LittleEndian\">\n";
    os << "  <ImageData WholeExtent=\"0 " << nx - 1 << " 0 " << ny - 1 << " 0 "
       << nz - 1 << "\"\n";
    os << "             Origin=\"" << org[0] << " " << org[1] << " " << org[2]
       << "\"\n";
    os << "             Spacing=\"" << spc[0] << " " << spc[1] << " " << spc[2]
       << "\">\n";
    os << "    <Piece Extent=\"0 " << nx - 1 << " 0 " << ny - 1 << " 0 "
       << nz - 1 << "\">\n";
    os << "      <CellData>\n";

    os << std::setprecision(std::numeric_limits<Real>::max_digits10);
    for (int c = 0; c < nc; ++c) {
      os << "        <DataArray type=\""
         << (sizeof(Real) == 8 ? "Float64" : "Float32") << "\" Name=\""
         << (c < static_cast<int>(compNames.size()) ? compNames[c]
                                                    : "c" + std::to_string(c))
         << "\" NumberOfComponents=\"1\" format=\"ascii\">\n";
      for (int k = 0; k < nz; ++k)
        for (int j = 0; j < ny; ++j)
          for (int i = 0; i < nx; ++i)
            os << at(c, bx.lo[0] + i, bx.lo[1] + j, bx.lo[2] + k)
               << ((i + 1) % nx == 0 ? "\n" : " ");
      os << "        </DataArray>\n";
    }

    os << "      </CellData>\n";
    os << "    </Piece>\n";
    os << "  </ImageData>\n";
    os << "</VTKFile>\n";

    mEntries.push_back(Entry{step, time, name, gbox});
    base += patchSize;
  }
}

void VTKWriter::finalize() {
  const std::string pvd = mBase + ".pvd";
  std::ofstream os(pvd);
  if (!os)
    throw std::runtime_error("VTKWriter: cannot open " + pvd);

  os << "<?xml version=\"1.0\"?>\n";
  os << "<VTKFile type=\"Collection\" version=\"0.1\" "
        "byte_order=\"LittleEndian\">\n";
  os << "  <Collection>\n";
  for (const auto& e : mEntries) {
    std::string f = e.file;
    const auto slash = f.find_last_of('/');
    if (slash != std::string::npos)
      f = f.substr(slash + 1);
    os << "    <DataSet timestep=\"" << e.step << "\" group=\"\" part=\""
       << e.part << "\" file=\"" << f << "\"/>\n";
  }
  os << "  </Collection>\n";
  os << "</VTKFile>\n";
}

}  // namespace saturn
