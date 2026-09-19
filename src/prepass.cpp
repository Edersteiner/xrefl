#include "prepass.h"

#include <algorithm>
#include <cctype>

namespace xrefl {
namespace {

bool is_ident_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$' ||
           static_cast<unsigned char>(c) >= 0x80;
}

bool is_ident_char(char c) { return is_ident_start(c) || (c >= '0' && c <= '9'); }

bool is_digit(char c) { return c >= '0' && c <= '9'; }

// A backslash at end of line continues the comment onto the next line.
uint32_t skip_line_comment(const std::string& t, uint32_t i) {
    i += 2;
    while (i < t.size()) {
        if (t[i] == '\\') {
            uint32_t j = i + 1;
            while (j < t.size() && (t[j] == ' ' || t[j] == '\t' || t[j] == '\r')) ++j;
            if (j < t.size() && t[j] == '\n') {
                i = j + 1;
                continue;
            }
        }
        if (t[i] == '\n') return i;
        ++i;
    }
    return i;
}

uint32_t skip_block_comment(const std::string& t, uint32_t i) {
    i += 2;
    while (i + 1 < t.size()) {
        if (t[i] == '*' && t[i + 1] == '/') return i + 2;
        ++i;
    }
    return static_cast<uint32_t>(t.size());
}

uint32_t skip_quoted(const std::string& t, uint32_t i) {
    char quote = t[i];
    ++i;
    while (i < t.size()) {
        if (t[i] == '\\') {
            i += 2;
            continue;
        }
        if (t[i] == quote) return i + 1;
        if (t[i] == '\n' && quote == '\'') return i;  // unterminated, do not run away
        ++i;
    }
    return i;
}

// `quote` is the offset of the opening quote in `R"delim(...)delim"`.
uint32_t skip_raw_string(const std::string& t, uint32_t quote) {
    uint32_t i = quote + 1;
    std::string delim;
    while (i < t.size() && t[i] != '(' && delim.size() <= 16) {
        delim.push_back(t[i]);
        ++i;
    }
    if (i >= t.size() || t[i] != '(') return i;
    std::string terminator = ")" + delim + "\"";
    size_t found = t.find(terminator, i);
    if (found == std::string::npos) return static_cast<uint32_t>(t.size());
    return static_cast<uint32_t>(found + terminator.size());
}

// Consumes a whole number so the digit separator in `1'000` is not taken for
// the start of a char literal.
uint32_t skip_number(const std::string& t, uint32_t i) {
    ++i;
    while (i < t.size()) {
        char c = t[i];
        if ((c == '+' || c == '-') && i > 0) {
            char p = t[i - 1];
            if (p == 'e' || p == 'E' || p == 'p' || p == 'P') {
                ++i;
                continue;
            }
            return i;
        }
        if (c == '\'' && i + 1 < t.size() && is_ident_char(t[i + 1])) {
            i += 2;
            continue;
        }
        if (is_ident_char(c) || c == '.') {
            ++i;
            continue;
        }
        return i;
    }
    return i;
}

bool is_raw_string_prefix(const std::string& s) {
    return s == "R" || s == "LR" || s == "uR" || s == "UR" || s == "u8R";
}

// Skips one comment, literal, number or identifier. Anything else advances by
// one byte.
uint32_t skip_element(const std::string& t, uint32_t i) {
    char c = t[i];
    if (c == '/' && i + 1 < t.size()) {
        if (t[i + 1] == '/') return skip_line_comment(t, i);
        if (t[i + 1] == '*') return skip_block_comment(t, i);
    }
    if (c == '"' || c == '\'') return skip_quoted(t, i);
    if (is_digit(c) || (c == '.' && i + 1 < t.size() && is_digit(t[i + 1]))) {
        return skip_number(t, i);
    }
    if (is_ident_start(c)) {
        uint32_t j = i;
        while (j < t.size() && is_ident_char(t[j])) ++j;
        if (j < t.size() && t[j] == '"' && is_raw_string_prefix(t.substr(i, j - i))) {
            return skip_raw_string(t, j);
        }
        return j;
    }
    return i + 1;
}

// Returns one past the `)` matching the `(` at `open`, or 0 if unbalanced.
uint32_t scan_balanced_parens(const std::string& t, uint32_t open) {
    int depth = 0;
    uint32_t i = open;
    while (i < t.size()) {
        char c = t[i];
        if (c == '(') {
            ++depth;
            ++i;
            continue;
        }
        if (c == ')') {
            --depth;
            ++i;
            if (depth == 0) return i;
            continue;
        }
        uint32_t next = skip_element(t, i);
        i = (next > i) ? next : i + 1;
    }
    return 0;
}

uint32_t skip_trivia(const std::string& t, uint32_t i) {
    while (i < t.size()) {
        char c = t[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v') {
            ++i;
            continue;
        }
        if (c == '/' && i + 1 < t.size() && t[i + 1] == '/') {
            i = skip_line_comment(t, i);
            continue;
        }
        if (c == '/' && i + 1 < t.size() && t[i + 1] == '*') {
            i = skip_block_comment(t, i);
            continue;
        }
        return i;
    }
    return i;
}

}  // namespace

Prepass run_prepass(const Source& source, const std::vector<std::string>& annotation_names,
                    const std::vector<std::string>& ignored_macros, DiagSink& diags) {
    Prepass result;
    const std::string& t = source.text();
    result.blanked = t;

    uint32_t i = 0;
    while (i < t.size()) {
        if (!is_ident_start(t[i])) {
            uint32_t next = skip_element(t, i);
            i = (next > i) ? next : i + 1;
            continue;
        }

        uint32_t name_begin = i;
        uint32_t name_end = i;
        while (name_end < t.size() && is_ident_char(t[name_end])) ++name_end;
        std::string word = t.substr(name_begin, name_end - name_begin);

        if (name_end < t.size() && t[name_end] == '"' && is_raw_string_prefix(word)) {
            i = skip_raw_string(t, name_end);
            continue;
        }

        bool known = std::find(annotation_names.begin(), annotation_names.end(), word) !=
                     annotation_names.end();
        bool ignored = !known && std::find(ignored_macros.begin(), ignored_macros.end(), word) !=
                                     ignored_macros.end();

        if (ignored) {
            // Blank the macro and its argument list if it has one.
            uint32_t stop = name_end;
            uint32_t paren = skip_trivia(t, name_end);
            if (paren < t.size() && t[paren] == '(') {
                uint32_t close = scan_balanced_parens(t, paren);
                if (close != 0) stop = close;
            }
            for (uint32_t k = name_begin; k < stop; ++k) {
                if (result.blanked[k] != '\n') result.blanked[k] = ' ';
            }
            i = stop;
            continue;
        }

        if (!known) {
            i = name_end;
            continue;
        }

        // Without parens it is an ordinary identifier.
        uint32_t paren = skip_trivia(t, name_end);
        if (paren >= t.size() || t[paren] != '(') {
            i = name_end;
            continue;
        }

        uint32_t close = scan_balanced_parens(t, paren);
        if (close == 0) {
            diags.error("unbalanced-annotation",
                        "unterminated argument list for annotation '" + word + "'",
                        "check for a missing ')' after the annotation", source, name_begin);
            i = name_end;
            continue;
        }

        AnnotationSite site;
        site.name = word;
        site.args = t.substr(paren + 1, close - paren - 2);
        site.begin = name_begin;
        site.end = close;
        result.sites.push_back(std::move(site));

        for (uint32_t k = name_begin; k < close; ++k) {
            if (result.blanked[k] != '\n') result.blanked[k] = ' ';
        }
        i = close;
    }

    // Resolved against the blanked text so that several annotations on one
    // declaration all point past the last of them.
    for (AnnotationSite& site : result.sites) {
        site.attaches_to = skip_trivia(result.blanked, site.end);
    }

    return result;
}

}  // namespace xrefl
