REFLECT()
enum class DoorState : uint8_t {
    Closed,
    Open = 3,
    Ajar = Open + 1,
};

REFLECT()
enum Legacy { A, B, C };

namespace deep { namespace inner {
REFLECT()
enum class Nested { One };
} }
