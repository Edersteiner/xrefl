#pragma once
#include <cstddef>

namespace xrefl {

struct FieldInfo {
    const char* name;
    std::size_t offset;
    std::size_t size;
    bool transient;
};

struct TypeInfo {
    const char* name;
    const char* category;
    const FieldInfo* fields;
    int field_count;
};

}  // namespace xrefl
