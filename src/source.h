#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace xrefl {

struct Location {
    uint32_t line = 0;    // 1-based
    uint32_t column = 0;  // 1-based, counted in bytes
};

class Source {
public:
    static bool load(const std::string& path, Source& out, std::string& error);

    static Source from_text(std::string path, std::string text);

    const std::string& path() const { return path_; }
    const std::string& text() const { return text_; }
    uint32_t size() const { return static_cast<uint32_t>(text_.size()); }

    Location locate(uint32_t offset) const;

    std::string_view slice(uint32_t begin, uint32_t end) const {
        if (begin > end || end > text_.size()) return {};
        return std::string_view(text_).substr(begin, end - begin);
    }

private:
    void index_lines();

    std::string path_;
    std::string text_;
    std::vector<uint32_t> line_starts_;  // byte offset of each line's first char
};

}  // namespace xrefl
