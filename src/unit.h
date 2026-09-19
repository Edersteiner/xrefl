#pragma once

#include <string>
#include <vector>

#include "source.h"

namespace xrefl {

// `args` is the text between the parens as written. It is Lua, and the plugin
// evaluates it.
struct AnnotationRef {
    std::string name;
    std::string args;
    Location loc;
};

struct Param {
    std::string name;  // empty for an unnamed parameter
    std::string type;
};

struct Field {
    std::string name;
    std::string type;  // as spelled, with the declarator's identifier elided
    std::string access;
    bool is_static = false;
    bool is_mutable = false;
    std::string bitfield;  // spelled width, empty when not a bitfield
    std::vector<AnnotationRef> annotations;
    Location loc;
};

struct Method {
    std::string name;
    std::string return_type;
    std::string access;
    std::vector<Param> params;
    bool is_static = false;
    bool is_virtual = false;
    bool is_const = false;
    bool is_pure = false;
    std::vector<AnnotationRef> annotations;
    Location loc;
};

// Recorded as spelled. Matching `Base` to `ns::Base` needs every type in the
// target, so the plugin does it.
struct Base {
    std::string name;
    std::string access;
    bool is_virtual = false;
};

struct Struct {
    std::string name;
    std::string ns;
    std::string kind;  // struct | class | union
    std::vector<Base> bases;
    std::vector<Field> fields;
    std::vector<Method> methods;
    std::vector<AnnotationRef> annotations;
    Location loc;
};

struct EnumValue {
    std::string name;
    std::string value;  // spelled initializer, empty when implicit
};

struct Enum {
    std::string name;
    std::string ns;
    std::string underlying;  // as spelled, empty when unspecified
    bool is_scoped = false;
    std::vector<EnumValue> values;
    std::vector<AnnotationRef> annotations;
    Location loc;
};

struct Function {
    std::string name;
    std::string ns;
    std::string return_type;
    std::vector<Param> params;
    std::vector<AnnotationRef> annotations;
    Location loc;
};

struct Unit {
    std::string path;
    std::vector<std::string> includes;
    std::vector<Struct> structs;
    std::vector<Enum> enums;
    std::vector<Function> functions;
};

}  // namespace xrefl
