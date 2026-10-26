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

VTKWriter::VTKWriter(std::string base) : m_base(std::move(base)) {
  const auto slash = m_base.find_last_of('/');
  if (slash != std::string::npos) {
    std::error_code ec;
    std::filesystem::create_directories(m_base.substr(0, slash), ec);
  }
}

void VTKWriter::write(const MultiFab& mf, const Geometry& geom, int step,
                      double time, const std::vector<std::string>& compNames) {
  const std::string tag = stepTag(step);

  for (int lb = 0; lb < mf.numLocalBoxes(); ++lb) {
    const int gbox = mf.localToGlobal(lb);
    const Box bx = mf.boxArray()[gbox];
    const Array4<Real> a = mf.array(lb);

    char name[512];
    std::snprintf(name, sizeof(name), "%s_%s_p%04d.vti", m_base.c_str(),
                  tag.c_str(), gbox);

    std::ofstream os(name);
    if (!os) throw std::runtime_error(std::string("VTKWriter: cannot open ") + name);

    // Extent in local (0-based) indices.
    const int nx = bx.length(0);
    const int ny = bx.length(1);
    const int nz = (NDIM == 3) ? bx.length(2) : 1;

    // Physical origin of the first cell centre minus half a cell.
    Real org[3] = {0, 0, 0};
    Real spc[3] = {geom.dx, geom.dx, static_cast<Real>(1)};
    for (int d = 0; d < NDIM; ++d)
      org[d] = geom.origin[d] +
               (static_cast<Real>(bx.lo[d] - geom.domain.lo[d])) * geom.dx;

    os << "<?xml version=\"1.0\"?>\n";
    os << "<VTKFile type=\"ImageData\" version=\"0.1\" byte_order=\"LittleEndian\">\n";
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
    for (int c = 0; c < mf.numComp(); ++c) {
      os << "        <DataArray type=\""
         << (sizeof(Real) == 8 ? "Float64" : "Float32")
         << "\" Name=\""
         << (c < static_cast<int>(compNames.size()) ? compNames[c]
                                                    : "c" + std::to_string(c))
         << "\" NumberOfComponents=\"1\" format=\"ascii\">\n";
      for (int k = 0; k < nz; ++k)
        for (int j = 0; j < ny; ++j)
          for (int i = 0; i < nx; ++i)
            os << a(bx.lo[0] + i, bx.lo[1] + j, bx.lo[2] + k, c)
               << ((i + 1) % nx == 0 ? "\n" : " ");
      os << "        </DataArray>\n";
    }

    os << "      </CellData>\n";
    os << "    </Piece>\n";
    os << "  </ImageData>\n";
    os << "</VTKFile>\n";

    m_entries.push_back(Entry{step, time, name, gbox});
  }
}

void VTKWriter::finalize() {
  const std::string pvd = m_base + ".pvd";
  std::ofstream os(pvd);
  if (!os) throw std::runtime_error("VTKWriter: cannot open " + pvd);

  os << "<?xml version=\"1.0\"?>\n";
  os << "<VTKFile type=\"Collection\" version=\"0.1\" byte_order=\"LittleEndian\">\n";
  os << "  <Collection>\n";
  for (const auto& e : m_entries) {
    // Strip the directory part: .pvd and .vti live side by side.
    std::string f = e.file;
    const auto slash = f.find_last_of('/');
    if (slash != std::string::npos) f = f.substr(slash + 1);
    os << "    <DataSet timestep=\"" << e.step << "\" group=\"\" part=\""
       << e.part << "\" file=\"" << f << "\"/>\n";
  }
  os << "  </Collection>\n";
  os << "</VTKFile>\n";
}

}  // namespace saturn
