#pragma once
#include <subdir/reflect_annotations.h>

namespace game {

REFLECT(category = "audio")
struct Speaker {
    PROPERTY(range = {0.5, 2.25}) float gain;
};

}  // namespace game
