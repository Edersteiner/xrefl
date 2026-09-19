REFLECT()
struct Types {
    PROPERTY() int plain;
    PROPERTY() const MeshHandle* pointer;
    PROPERTY() float& reference;
    PROPERTY() double&& rvalue;
    PROPERTY() int array[4];
    PROPERTY() char grid[2][3];
    PROPERTY() int* pointers[8];
    PROPERTY() void (*callback)(int, float);
    PROPERTY() unsigned int flags : 3;
    PROPERTY() static int shared;
    PROPERTY() mutable bool dirty;
    PROPERTY() std::vector<std::pair<int, float>> nested;
    PROPERTY() glm::vec3 position;
    PROPERTY() ns::Deep::Inner value;
};
