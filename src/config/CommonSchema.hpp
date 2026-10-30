#pragma once

#include "config/ConfigSchema.hpp"

namespace saturn {

/// Schema for the common groups every case shares. Solvers add their own keys
/// to a copy of this schema in Phase 2.
ConfigSchema commonSchema();

}  // namespace saturn
