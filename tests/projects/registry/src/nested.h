#pragma once
#include <registry/reflect_annotations.h>
#include "entities.h"

namespace game { namespace world {

// Base in an enclosing namespace.
REFLECT()
struct Portal : Entity {
    PROPERTY() int destination;
};

}}  // namespace game::world
