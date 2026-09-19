#pragma once
#include <errors/reflect_annotations.h>

struct Thing {
    PROPERTY(rnage = {0, 1}) float value;
};
