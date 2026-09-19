#pragma once

#include <string>

#include "unit.h"

namespace xrefl {

// Keys are written in a fixed order so the output diffs cleanly.
std::string to_json(const Unit& unit, bool pretty);

}  // namespace xrefl
