#pragma once

#include "coupecad/core/project.h"

namespace coupecad::app {

// 1200×600×2000 mm cabinet with bottom + top + 2 sides + back + 2 shelves.
// No hardware (Stage 4a keeps the surface minimal).
core::Project make_demo_project();

}  // namespace coupecad::app
