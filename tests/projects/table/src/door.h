#pragma once
#include <table/reflect_annotations.h>
#include <cstdint>

namespace game {

REFLECT(category = "world")
struct Door {
    PROPERTY() std::uint8_t state;
    PROPERTY(range = {0.1, 20.0}) float speed;
    float animCursor;  // unannotated, invisible to reflection
};

REFLECT()
struct Lamp {
    PROPERTY() bool on;
    PROPERTY(transient) float flicker;
};

}  // namespace game
