/**
 * @file VTKWriter.hpp
 *
 * saturnCFD — ParaView-friendly output.
 *
 * Writes one XML ImageData (.vti) file per box per timestep, plus a .pvd
 * collection so ParaView can load the whole AMR hierarchy / all times at once.
 *
 * v0.1 writes ASCII (readable, diffable, slow for large runs). Binary appended
 * is a drop-in upgrade; the file layout does not change.
 */
#pragma once

#include <string>
#include <vector>

#include "amr/MultiFab.hpp"
#include "core/Types.hpp"
#include "mesh/Geometry.hpp"

namespace saturn {

/** @brief ParaView output: one .vti per patch plus a .pvd collection. */
class VTKWriter {
 public:
  /// `base` is a path prefix, e.g. "output/field" -> output/field.pvd.
  explicit VTKWriter(std::string base);

  /// Write all LOCAL boxes of `mf` at this step. One .vti per box.
  /// `compNames` lets ParaView show "u", "v" ... instead of "c0", "c1".
  void write(const MultiFab& mf, const Geometry& geom, int step, double time,
             const std::vector<std::string>& compNames = {});

  /// Flush the .pvd collection index. Call once at the end of the run.
  void finalize();

 private:
  struct Entry {
    int step;
    double time;
    std::string file;
    int part;
  };

  std::string mBase;
  std::vector<Entry> mEntries;
};

}  // namespace saturn
