#pragma once

// Runtime support for emit_registry.lua. Copy it alongside the emitter and
// change it. Nothing in xrefl depends on these types.

#include <cstddef>
#include <cstring>

#include <xrefl/type_id.h>

namespace xrefl {

struct FieldInfo {
    const char* name;
    const char* type;  // as spelled in the header
    TypeId type_id;
    std::size_t offset;
    std::size_t size;

    template <typename Owner, typename T>
    T* get(Owner* owner) const {
        return type_id == xrefl::type_id<T>()
                   ? reinterpret_cast<T*>(reinterpret_cast<char*>(owner) + offset)
                   : nullptr;
    }
};

struct TypeInfo {
    const char* name;  // qualified, as the C++ compiler would spell it
    TypeId id;
    std::size_t size;
    const TypeInfo* const* bases;
    int base_count;
    const FieldInfo* fields;
    int field_count;

    const FieldInfo* find_field(const char* wanted) const {
        for (int i = 0; i < field_count; ++i) {
            if (std::strcmp(fields[i].name, wanted) == 0) {
                return &fields[i];
            }
        }
        return nullptr;
    }

    // Only reflected bases are recorded, so this answers "is it a reflected
    // ancestor", not "is it a C++ base".
    bool derives_from(const TypeInfo& ancestor) const {
        if (this == &ancestor || id == ancestor.id) {
            return true;
        }
        for (int i = 0; i < base_count; ++i) {
            if (bases[i]->derives_from(ancestor)) {
                return true;
            }
        }
        return false;
    }
};

// Every type one target reflected.
struct Registry {
    const TypeInfo* const* types;
    int count;

    const TypeInfo* find(const char* name) const {
        for (int i = 0; i < count; ++i) {
            if (std::strcmp(types[i]->name, name) == 0) {
                return types[i];
            }
        }
        return nullptr;
    }
};

}  // namespace xrefl
