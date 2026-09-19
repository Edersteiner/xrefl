#pragma once
#include <engine/api.h>
#include <engine/reflect_annotations.h>

namespace engine {

ENGINE_API
REFLECT(category = "engine")
struct Handle {
    PROPERTY() unsigned int index;
    PROPERTY() unsigned int generation;
};

}  // namespace engine
