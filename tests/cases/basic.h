#include <myproject/reflect_annotations.h>
#include "engine/handle.h"

namespace game {

REFLECT(category = "world")
struct Door : public Entity {
    PROPERTY() uint8_t state;
    PROPERTY(range = {0.1, 20.0}) float speed;
    PROPERTY(asset = "mesh") MeshHandle mesh;
    float animCursor;

    METHOD(script_exposed = true) void Open(float t);
};

}
