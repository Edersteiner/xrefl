#include "diag.h"

namespace xrefl {

std::string Diagnostic::format() const {
    std::string out = path;
    out += ':';
    out += std::to_string(loc.line);
    out += ':';
    out += std::to_string(loc.column);
    out += (severity == Severity::Error) ? ": error: " : ": warning: ";
    out += message;
    out += " [";
    out += code;
    out += ']';
    if (!note.empty()) {
        out += "\n  note: ";
        out += note;
    }
    return out;
}

void DiagSink::report(Severity severity, std::string code, std::string message, std::string note,
                      const Source& source, uint32_t offset) {
    Diagnostic d;
    d.severity = severity;
    d.code = std::move(code);
    d.message = std::move(message);
    d.note = std::move(note);
    d.path = source.path();
    d.loc = source.locate(offset);
    diagnostics_.push_back(std::move(d));
    if (severity == Severity::Error) ++error_count_;
}

}  // namespace xrefl
