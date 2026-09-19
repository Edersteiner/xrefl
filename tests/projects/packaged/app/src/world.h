#pragma once
#include <app/reflect_annotations.h>
#include <cstdint>

namespace game {

REFLECT(category = "world")
struct Chunk {
    PROPERTY() std::int32_t x;
    PROPERTY() std::int32_t y;
    PROPERTY() float density;
};

}  // namespace game
