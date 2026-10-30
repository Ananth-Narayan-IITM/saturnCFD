#include "config/CommonSchema.hpp"

#include "core/Units.hpp"

namespace saturn {

ConfigSchema commonSchema() {
  ConfigSchema s;
  s.addString("solver", "", "solver name (required)");
  s.addEnum("units.system", "SI", {"SI"}, "unit system; v1.0 accepts SI only");
  s.addString("mesh.file", "", "Gmsh .msh file");
  s.addEnum("mesh.dimension", "auto", {"auto", "1", "2", "3"},
            "mesh dimension; auto detects one-cell-thick meshes");
  s.addDouble("time.start", 0.0, "start time [s]", Dimension::Time);
  s.addDouble("time.end", 1.0, "end time [s]", Dimension::Time);
  s.addInt("time.maxSteps", 10000, "hard step limit", 1, 1000000000);
  s.addDouble("time.cfl", 0.4, "convective CFL", Dimension::Dimensionless, 0.0,
              1.0, true);
  s.addString("output.prefix", "results", "output path prefix");
  s.addInt("output.vtkEvery", 0, "write VTK every N steps", 0, 1000000000);
  s.require("solver");
  return s;
}

}  // namespace saturn
