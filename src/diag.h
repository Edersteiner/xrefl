#pragma once

#include <string>
#include <vector>

#include "source.h"

namespace xrefl {

enum class Severity { Warning, Error };

// `code` is a stable kebab-case name for the kind of failure. `note` says
// what to do about it.
struct Diagnostic {
    Severity severity = Severity::Error;
    std::string code;
    std::string message;
    std::string note;
    std::string path;
    Location loc;

    std::string format() const;
};

class DiagSink {
public:
    void report(Severity severity, std::string code, std::string message, std::string note,
                const Source& source, uint32_t offset);

    void error(std::string code, std::string message, std::string note, const Source& source,
               uint32_t offset) {
        report(Severity::Error, std::move(code), std::move(message), std::move(note), source,
               offset);
    }

    void warn(std::string code, std::string message, std::string note, const Source& source,
              uint32_t offset) {
        report(Severity::Warning, std::move(code), std::move(message), std::move(note), source,
               offset);
    }

    const std::vector<Diagnostic>& diagnostics() const { return diagnostics_; }
    bool has_errors() const { return error_count_ > 0; }
    size_t error_count() const { return error_count_; }

private:
    std::vector<Diagnostic> diagnostics_;
    size_t error_count_ = 0;
};

}  // namespace xrefl
