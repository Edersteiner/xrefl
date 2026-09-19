#pragma once
#include <game/reflect_annotations.h>

namespace game {

REFLECT(category = "world")
struct Door {
    PROPERTY() int state;
    PROPERTY(asset = "mesh") int mesh;
    float internal;
};

}  // namespace game
