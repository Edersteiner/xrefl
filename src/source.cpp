#include "source.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>

namespace xrefl {

bool Source::load(const std::string& path, Source& out, std::string& error) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        error = std::strerror(errno);
        return false;
    }
    std::string text;
    char buffer[65536];
    size_t n;
    while ((n = std::fread(buffer, 1, sizeof(buffer), f)) > 0) {
        text.append(buffer, n);
    }
    bool failed = std::ferror(f) != 0;
    std::fclose(f);
    if (failed) {
        error = "read failed";
        return false;
    }
    out = from_text(path, std::move(text));
    return true;
}

Source Source::from_text(std::string path, std::string text) {
    Source s;
    s.path_ = std::move(path);
    s.text_ = std::move(text);
    s.index_lines();
    return s;
}

void Source::index_lines() {
    line_starts_.clear();
    line_starts_.push_back(0);
    for (uint32_t i = 0; i < text_.size(); ++i) {
        if (text_[i] == '\n') line_starts_.push_back(i + 1);
    }
}

Location Source::locate(uint32_t offset) const {
    if (line_starts_.empty()) return {1, 1};
    offset = std::min<uint32_t>(offset, static_cast<uint32_t>(text_.size()));
    auto it = std::upper_bound(line_starts_.begin(), line_starts_.end(), offset);
    uint32_t line = static_cast<uint32_t>(it - line_starts_.begin());
    uint32_t line_start = line_starts_[line - 1];
    return {line, offset - line_start + 1};
}

}  // namespace xrefl
