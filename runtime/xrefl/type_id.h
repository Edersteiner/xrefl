#pragma once

namespace xrefl {

// Type identity without RTTI. Each instantiation owns one byte whose address
// is unique for the life of the program.
using TypeId = const void*;

template <typename T>
TypeId type_id() {
    static const char storage = 0;
    return &storage;
}

}  // namespace xrefl
