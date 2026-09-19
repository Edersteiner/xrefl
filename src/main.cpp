#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "diag.h"
#include "json.h"
#include "parse.h"
#include "source.h"

namespace {

const char* kUsage =
    "usage: xrefl parse [options] <header>...\n"
    "\n"
    "options:\n"
    "  --annotations A,B,C   comma-separated annotation names to look for\n"
    "  --annotation NAME     add one annotation name (repeatable)\n"
    "  --ignore-macros A,B   comma-separated macros to blank before parsing\n"
    "  --ignore-macro NAME   add one macro to blank (repeatable)\n"
    "  --out-dir DIR         write <header>.json under DIR, mirroring --root\n"
    "  --root DIR            paths are recorded relative to this directory\n"
    "  --stdout              write the unit to stdout (one header only)\n"
    "  --pretty              indent the JSON\n"
    "  -h, --help            show this message\n"
    "  --version             print the version\n";

struct Options {
    std::vector<std::string> annotations;
    std::vector<std::string> ignored_macros;
    std::vector<std::string> headers;
    std::string out_dir;
    std::string root;
    bool to_stdout = false;
    bool pretty = false;
};

void split_commas(const std::string& in, std::vector<std::string>& out) {
    size_t begin = 0;
    while (begin <= in.size()) {
        size_t comma = in.find(',', begin);
        std::string piece = in.substr(begin, comma == std::string::npos ? std::string::npos
                                                                       : comma - begin);
        if (!piece.empty()) out.push_back(piece);
        if (comma == std::string::npos) break;
        begin = comma + 1;
    }
}

std::string normalize_separators(std::string path) {
    for (char& c : path) {
        if (c == '\\') c = '/';
    }
    return path;
}

// Paths are recorded relative to --root so output does not depend on where
// the project is checked out.
std::string relative_to(const std::string& root, const std::string& path) {
    if (root.empty()) return path;
    std::string prefix = root;
    if (!prefix.empty() && prefix.back() != '/') prefix += '/';
    if (path.rfind(prefix, 0) == 0) return path.substr(prefix.size());
    return path;
}

// Joins the path segments with dots so headers with the same file name in
// different directories do not collide. Leading separators, drive letters and
// `.`/`..` are dropped.
std::string output_path(const std::string& out_dir, const std::string& relative) {
    std::string flattened;
    size_t begin = 0;
    while (begin <= relative.size()) {
        size_t slash = relative.find('/', begin);
        size_t end = (slash == std::string::npos) ? relative.size() : slash;
        std::string segment = relative.substr(begin, end - begin);
        bool skip = segment.empty() || segment == "." || segment == "..";
        if (!skip && segment.size() == 2 && segment[1] == ':') skip = true;  // C:
        if (!skip) {
            if (!flattened.empty()) flattened += '.';
            flattened += segment;
        }
        if (slash == std::string::npos) break;
        begin = slash + 1;
    }
    if (flattened.empty()) flattened = "unit";
    return out_dir + "/" + flattened + ".json";
}

bool make_directories(const std::string& path) {
    std::string current;
    for (size_t i = 0; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == '/') {
            if (!current.empty()) {
#ifdef _WIN32
                _mkdir(current.c_str());
#else
                mkdir(current.c_str(), 0755);
#endif
            }
        }
        if (i < path.size()) current += path[i];
    }
    return true;
}

bool write_file(const std::string& path, const std::string& content) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    size_t written = std::fwrite(content.data(), 1, content.size(), f);
    bool ok = written == content.size() && std::ferror(f) == 0;
    std::fclose(f);
    return ok;
}

int fail(const std::string& message) {
    std::fprintf(stderr, "xrefl: error: %s\n", message.c_str());
    return 1;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fputs(kUsage, stderr);
        return 2;
    }

    int index = 1;
    if (std::strcmp(argv[1], "-h") == 0 || std::strcmp(argv[1], "--help") == 0) {
        std::fputs(kUsage, stdout);
        return 0;
    }
    if (std::strcmp(argv[1], "--version") == 0) {
        std::printf("xrefl %s\n", XREFL_VERSION);
        return 0;
    }
    if (std::strcmp(argv[1], "parse") != 0) {
        return fail(std::string("unknown command '") + argv[1] + "'");
    }
    ++index;

    Options options;
    for (; index < argc; ++index) {
        std::string arg = argv[index];
        auto next = [&](const char* name) -> const char* {
            if (index + 1 >= argc) {
                std::fprintf(stderr, "xrefl: error: %s needs a value\n", name);
                std::exit(2);
            }
            return argv[++index];
        };
        if (arg == "--annotations") {
            split_commas(next("--annotations"), options.annotations);
        } else if (arg == "--annotation") {
            options.annotations.push_back(next("--annotation"));
        } else if (arg == "--ignore-macros") {
            split_commas(next("--ignore-macros"), options.ignored_macros);
        } else if (arg == "--ignore-macro") {
            options.ignored_macros.push_back(next("--ignore-macro"));
        } else if (arg == "--out-dir") {
            options.out_dir = normalize_separators(next("--out-dir"));
        } else if (arg == "--root") {
            options.root = normalize_separators(next("--root"));
        } else if (arg == "--stdout") {
            options.to_stdout = true;
        } else if (arg == "--pretty") {
            options.pretty = true;
        } else if (arg == "-h" || arg == "--help") {
            std::fputs(kUsage, stdout);
            return 0;
        } else if (!arg.empty() && arg[0] == '-') {
            return fail("unknown option '" + arg + "'");
        } else {
            options.headers.push_back(normalize_separators(arg));
        }
    }

    if (options.headers.empty()) return fail("no headers given");
    if (options.to_stdout && options.headers.size() != 1) {
        return fail("--stdout takes exactly one header");
    }
    if (!options.to_stdout && options.out_dir.empty()) {
        return fail("either --out-dir or --stdout is required");
    }
    if (!options.out_dir.empty()) make_directories(options.out_dir);

    size_t errors = 0;
    for (const std::string& header : options.headers) {
        xrefl::Source source;
        std::string load_error;
        std::string relative = relative_to(options.root, header);
        if (!xrefl::Source::load(header, source, load_error)) {
            std::fprintf(stderr, "xrefl: error: %s: %s\n", header.c_str(), load_error.c_str());
            ++errors;
            continue;
        }
        source = xrefl::Source::from_text(relative, source.text());

        xrefl::Unit unit;
        xrefl::DiagSink diags;
        xrefl::parse_unit(source, options.annotations, options.ignored_macros, unit, diags);

        for (const xrefl::Diagnostic& d : diags.diagnostics()) {
            std::fprintf(stderr, "%s\n", d.format().c_str());
        }
        if (diags.has_errors()) {
            errors += diags.error_count();
            continue;
        }

        std::string json = xrefl::to_json(unit, options.pretty || options.to_stdout);
        if (options.to_stdout) {
            std::fwrite(json.data(), 1, json.size(), stdout);
        } else if (!write_file(output_path(options.out_dir, relative), json)) {
            std::fprintf(stderr, "xrefl: error: cannot write output for %s\n", header.c_str());
            ++errors;
        }
    }

    return errors == 0 ? 0 : 1;
}
