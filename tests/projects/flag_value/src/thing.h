#pragma once
#include <flag_value/reflect_annotations.h>

struct Thing {
    PROPERTY(transient = 3) float value;
};
