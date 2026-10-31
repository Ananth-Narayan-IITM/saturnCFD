#include "config/CommonSchema.hpp"

#include "core/Units.hpp"

namespace saturn {

ConfigSchema commonSchema() {
  ConfigSchema s;
  s.addString("solver", "", "solver name (required)");
  s.addEnum("units.system", "SI", {"SI"}, "unit system; v1.0 accepts SI only");
  s.addEnum("mesh.type", "auto", {"auto", "gmsh", "box"},
            "mesh source: Gmsh file or structured box");
  s.addString("mesh.file", "", "Gmsh .msh file");
  s.addEnum("mesh.dimension", "auto", {"auto", "1", "2", "3"},
            "mesh dimension; auto detects one-cell-thick meshes");
  s.addInt("mesh.nx", 1, "box cells in x", 1, 1000000000);
  s.addInt("mesh.ny", 1, "box cells in y", 1, 1000000000);
  s.addInt("mesh.nz", 1, "box cells in z", 1, 1000000000);
  s.addDouble("mesh.box.xmin", 0.0, "box lower x [m]", Dimension::Length);
  s.addDouble("mesh.box.xmax", 1.0, "box upper x [m]", Dimension::Length);
  s.addDouble("mesh.box.ymin", 0.0, "box lower y [m]", Dimension::Length);
  s.addDouble("mesh.box.ymax", 1.0, "box upper y [m]", Dimension::Length);
  s.addDouble("mesh.box.zmin", 0.0, "box lower z [m]", Dimension::Length);
  s.addDouble("mesh.box.zmax", 1.0, "box upper z [m]", Dimension::Length);
  s.addDouble("time.start", 0.0, "start time [s]", Dimension::Time);
  s.addDouble("time.end", 1.0, "end time [s]", Dimension::Time);
  s.addDouble("time.dtMax", 1e-2, "largest explicit step [s]", Dimension::Time);
  s.addInt("time.maxSteps", 10000, "hard step limit", 1, 1000000000);
  s.addDouble("time.cfl", 0.4, "convective CFL", Dimension::Dimensionless, 0.0,
              1.0, true);
  s.addBool("time.adaptive", true, "adapt dt from the stability limit");
  s.addEnum("time.scheme", "ssprk3", {"forwardEuler", "ssprk2", "ssprk3"},
            "explicit time integration scheme");
  s.addString("output.prefix", "results", "output path prefix");
  s.addInt("output.vtkEvery", 0, "write VTK every N steps", 0, 1000000000);
  s.addBool("output.csv", false, "write a final profile as CSV");
  s.addBool("output.json", true, "write a JSON run summary");
  s.addBool("output.exact", false, "compare against the solver's exact data");
  s.require("solver");
  return s;
}

}  // namespace saturn
