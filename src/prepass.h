#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "diag.h"
#include "source.h"

namespace xrefl {

struct AnnotationSite {
    std::string name;
    std::string args;        // text between the parens, verbatim and untrimmed
    uint32_t begin = 0;      // offset of the first character of the name
    uint32_t end = 0;        // one past the closing paren
    uint32_t attaches_to = 0;  // offset of the next non-whitespace byte after `end`
    bool attached = false;   // set once a declaration claims it
};

struct Prepass {
    std::vector<AnnotationSite> sites;
    // The source with every annotation overwritten by spaces. Newlines are
    // kept, so offsets and line numbers match the original.
    std::string blanked;
};

// Blanks annotations so tree-sitter never sees a macro in front of a
// declaration. `ignored_macros` are blanked the same way but not recorded.
Prepass run_prepass(const Source& source, const std::vector<std::string>& annotation_names,
                    const std::vector<std::string>& ignored_macros, DiagSink& diags);

}  // namespace xrefl
