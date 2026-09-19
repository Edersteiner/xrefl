#pragma once
#include <json/reflect_annotations.h>

#include <string>

namespace shapes {

// Root of a polymorphic hierarchy. Needs a virtual destructor, and every
// concrete type must be default constructible.
REFLECT(polymorphic)
struct Shape {
    virtual ~Shape() = default;
    PROPERTY() std::string name;
};

// Derived types inherit the polymorphic marking.
REFLECT()
struct Circle : Shape {
    PROPERTY() double radius = 1.0;
};

REFLECT()
struct Rect : Shape {
    PROPERTY() double width = 1.0;
    PROPERTY() double height = 1.0;
};

// Second level, so the upcast chain has more than one step.
REFLECT()
struct Square : Rect {
    PROPERTY() bool locked = false;
};

// Reflected but not a Shape. Loading it as one must fail.
REFLECT(polymorphic)
struct Marker {
    virtual ~Marker() = default;
    PROPERTY() int index = 0;
};

}  // namespace shapes
