#pragma once
#include <registry/reflect_annotations.h>
#include <cstdint>

namespace game {

REFLECT()
struct Entity {
    PROPERTY() std::uint32_t id;
    PROPERTY() float x;
    PROPERTY() float y;
};

// Base spelled without qualification.
REFLECT()
struct Door : Entity {
    PROPERTY() std::uint8_t state;
    PROPERTY() float speed;
    float animCursor;               // unannotated
    static int instances;           // static: no offset within an instance
    PROPERTY() unsigned bits : 3;   // bitfield: no address
};

}  // namespace game
