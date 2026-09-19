#pragma once

#include <string>
#include <vector>

#include "diag.h"
#include "source.h"
#include "unit.h"

namespace xrefl {

// Returns false only when the header cannot be parsed at all. A bad
// declaration reports through `diags` and the rest of the unit is still
// filled in.
bool parse_unit(const Source& source, const std::vector<std::string>& annotation_names,
                const std::vector<std::string>& ignored_macros, Unit& unit, DiagSink& diags);

}  // namespace xrefl
