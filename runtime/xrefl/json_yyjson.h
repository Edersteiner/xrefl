#pragma once

// Runtime support for emit_json_yyjson.lua. Copy it alongside the emitter and
// change it. Fields go through `write` and `read` below, and overload
// resolution picks the function for the type.

#include <yyjson.h>

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#ifndef XREFL_JSON_NO_RTTI
#include <typeinfo>
#endif

#include <xrefl/type_id.h>

namespace xrefl {
namespace json {
namespace detail {

// Found by unqualified lookup from the dispatchers below, alongside whatever
// ADL finds in the value's own namespace.

inline yyjson_mut_val* xrefl_to_json(yyjson_mut_doc* doc, bool v) {
    return yyjson_mut_bool(doc, v);
}
inline yyjson_mut_val* xrefl_to_json(yyjson_mut_doc* doc, double v) {
    return yyjson_mut_real(doc, v);
}
inline yyjson_mut_val* xrefl_to_json(yyjson_mut_doc* doc, float v) {
    return yyjson_mut_real(doc, static_cast<double>(v));
}
inline yyjson_mut_val* xrefl_to_json(yyjson_mut_doc* doc, std::int64_t v) {
    return yyjson_mut_sint(doc, v);
}
inline yyjson_mut_val* xrefl_to_json(yyjson_mut_doc* doc, std::uint64_t v) {
    return yyjson_mut_uint(doc, v);
}
inline yyjson_mut_val* xrefl_to_json(yyjson_mut_doc* doc, const std::string& v) {
    return yyjson_mut_strncpy(doc, v.c_str(), v.size());
}

// Every other integer type goes through the widest signed or unsigned one.
template <typename T, typename = typename std::enable_if<std::is_integral<T>::value &&
                                                         !std::is_same<T, bool>::value>::type>
yyjson_mut_val* xrefl_to_json(yyjson_mut_doc* doc, T v) {
    return std::is_signed<T>::value ? yyjson_mut_sint(doc, static_cast<std::int64_t>(v))
                                    : yyjson_mut_uint(doc, static_cast<std::uint64_t>(v));
}

inline bool xrefl_from_json(yyjson_val* v, bool& out) {
    if (!yyjson_is_bool(v)) return false;
    out = yyjson_get_bool(v);
    return true;
}
inline bool xrefl_from_json(yyjson_val* v, double& out) {
    if (!yyjson_is_num(v)) return false;
    out = yyjson_get_num(v);
    return true;
}
inline bool xrefl_from_json(yyjson_val* v, float& out) {
    if (!yyjson_is_num(v)) return false;
    out = static_cast<float>(yyjson_get_num(v));
    return true;
}
inline bool xrefl_from_json(yyjson_val* v, std::string& out) {
    const char* s = yyjson_get_str(v);
    if (!s) return false;
    out.assign(s, yyjson_get_len(v));
    return true;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value &&
                                                         !std::is_same<T, bool>::value>::type>
bool xrefl_from_json(yyjson_val* v, T& out) {
    if (!yyjson_is_num(v)) return false;
    out = static_cast<T>(std::is_signed<T>::value ? yyjson_get_sint(v)
                                                  : static_cast<std::int64_t>(yyjson_get_uint(v)));
    return true;
}

template <typename T>
yyjson_mut_val* xrefl_to_json(yyjson_mut_doc* doc, const std::vector<T>& v);
template <typename T>
bool xrefl_from_json(yyjson_val* v, std::vector<T>& out);

template <typename T>
yyjson_mut_val* dispatch_write(yyjson_mut_doc* doc, const T& v) {
    return xrefl_to_json(doc, v);
}

template <typename T>
bool dispatch_read(yyjson_val* v, T& out) {
    return xrefl_from_json(v, out);
}

template <typename T>
yyjson_mut_val* xrefl_to_json(yyjson_mut_doc* doc, const std::vector<T>& v) {
    yyjson_mut_val* array = yyjson_mut_arr(doc);
    for (const T& element : v) {
        yyjson_mut_arr_append(array, dispatch_write(doc, element));
    }
    return array;
}

template <typename T>
bool xrefl_from_json(yyjson_val* v, std::vector<T>& out) {
    if (!yyjson_is_arr(v)) return false;
    out.clear();
    out.reserve(yyjson_arr_size(v));
    size_t index, max;
    yyjson_val* element;
    yyjson_arr_foreach(v, index, max, element) {
        T value{};
        if (!dispatch_read(element, value)) return false;
        out.push_back(std::move(value));
    }
    return true;
}

}  // namespace detail

template <typename T>
yyjson_mut_val* write(yyjson_mut_doc* doc, const T& value) {
    return detail::dispatch_write(doc, value);
}

template <typename T>
bool read(yyjson_val* value, T& out) {
    return detail::dispatch_read(value, out);
}

template <typename T>
std::string to_string(const T& value, bool pretty = false) {
    yyjson_mut_doc* doc = yyjson_mut_doc_new(nullptr);
    yyjson_mut_doc_set_root(doc, write(doc, value));
    char* text = yyjson_mut_write(doc, pretty ? YYJSON_WRITE_PRETTY : 0, nullptr);
    std::string out = text ? text : "";
    if (text) free(text);
    yyjson_mut_doc_free(doc);
    return out;
}

template <typename T>
bool from_string(const std::string& text, T& out) {
    yyjson_doc* doc = yyjson_read(text.c_str(), text.size(), 0);
    if (!doc) return false;
    bool ok = read(yyjson_doc_get_root(doc), out);
    yyjson_doc_free(doc);
    return ok;
}

inline const char* discriminator() { return "$type"; }

// Plain function pointers, so the table is static data with no
// initialisation order.
struct PolymorphicEntry {
    const char* name;
    TypeId id;
    void* (*create)();
    void (*destroy)(void*);
    bool (*read)(yyjson_val*, void*);
    yyjson_mut_val* (*write)(yyjson_mut_doc*, const void*);
    // Needs no object, so a mismatch is rejected before construction.
    bool (*derives)(TypeId);
    // The address of `target` within the object. A base is not always at
    // offset zero, so each step casts where both types are complete.
    void* (*upcast)(void*, TypeId);
#ifndef XREFL_JSON_NO_RTTI
    const std::type_info* rtti;
#endif
};

struct PolymorphicRegistry {
    const PolymorphicEntry* entries;
    int count;

    const PolymorphicEntry* find(const char* name) const {
        if (!name) return nullptr;
        for (int i = 0; i < count; ++i) {
            if (std::strcmp(entries[i].name, name) == 0) return &entries[i];
        }
        return nullptr;
    }

#ifndef XREFL_JSON_NO_RTTI
    const PolymorphicEntry* find(const std::type_info& type) const {
        for (int i = 0; i < count; ++i) {
            if (entries[i].rtti && *entries[i].rtti == type) return &entries[i];
        }
        return nullptr;
    }
#endif
};

// Reads an object whose concrete type the document names. Checks it derives
// from `Base` before constructing anything.
template <typename Base>
std::unique_ptr<Base> read_polymorphic(const PolymorphicRegistry& registry, yyjson_val* obj) {
    static_assert(std::has_virtual_destructor<Base>::value,
                  "a polymorphic base needs a virtual destructor to be owned by unique_ptr");
    if (!yyjson_is_obj(obj)) return nullptr;

    const PolymorphicEntry* entry = registry.find(yyjson_get_str(
        yyjson_obj_get(obj, discriminator())));
    if (!entry || !entry->derives(type_id<Base>())) return nullptr;

    void* raw = entry->create();
    if (!raw) return nullptr;

    if (!entry->read(obj, raw)) {
        entry->destroy(raw);
        return nullptr;
    }
    return std::unique_ptr<Base>(static_cast<Base*>(entry->upcast(raw, type_id<Base>())));
}

template <typename Base>
std::unique_ptr<Base> load_polymorphic(const PolymorphicRegistry& registry,
                                       const std::string& text) {
    yyjson_doc* doc = yyjson_read(text.c_str(), text.size(), 0);
    if (!doc) return nullptr;
    std::unique_ptr<Base> out = read_polymorphic<Base>(registry, yyjson_doc_get_root(doc));
    yyjson_doc_free(doc);
    return out;
}

#ifndef XREFL_JSON_NO_RTTI
// Writes through a base reference using the concrete type. Needs RTTI, since
// nothing else recovers the derived type without a virtual in the user's own
// type. Define XREFL_JSON_NO_RTTI to leave it out.
template <typename Base>
std::string save_polymorphic(const PolymorphicRegistry& registry, const Base& value,
                             bool pretty = false) {
    const PolymorphicEntry* entry = registry.find(typeid(value));
    if (!entry) return "";

    yyjson_mut_doc* doc = yyjson_mut_doc_new(nullptr);
    yyjson_mut_doc_set_root(doc, entry->write(doc, static_cast<const void*>(&value)));
    char* text = yyjson_mut_write(doc, pretty ? YYJSON_WRITE_PRETTY : 0, nullptr);
    std::string out = text ? text : "";
    if (text) free(text);
    yyjson_mut_doc_free(doc);
    return out;
}
#endif

}  // namespace json
}  // namespace xrefl
