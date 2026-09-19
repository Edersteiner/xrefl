#pragma once
#include <json/reflect_annotations.h>

#include <yyjson.h>

#include <cstdint>
#include <string>
#include <vector>

namespace game {

REFLECT()
enum class DoorState : std::uint8_t { Closed, Open, Ajar };

// Not reflected. The hand-written pair is declared here because generated
// code only sees what this header includes.
struct Colour {
    float r = 0.0f, g = 0.0f, b = 0.0f;
};

yyjson_mut_val* xrefl_to_json(yyjson_mut_doc* doc, const Colour& value);
bool xrefl_from_json(yyjson_val* value, Colour& out);

REFLECT()
struct Entity {
    PROPERTY() std::uint32_t id = 0;
    PROPERTY() std::string name;
};

REFLECT()
struct Door : Entity {
    PROPERTY() DoorState state = DoorState::Closed;
    PROPERTY() float speed = 1.0f;
    PROPERTY() Colour tint;                       // reaches the hand-written pair
    PROPERTY() std::vector<std::string> tags;     // container of a leaf
    PROPERTY(transient) float animCursor = 0.0f;
};

REFLECT()
struct Level {
    PROPERTY() std::string title;
    PROPERTY() std::vector<Door> doors;           // container of a reflected type
};

}  // namespace game
