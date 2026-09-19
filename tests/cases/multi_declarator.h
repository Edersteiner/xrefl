REFLECT()
struct Multi {
    PROPERTY() float x, y, z;
    PROPERTY() int a, *b, c[3], **d;
    PROPERTY() const char *first, *second;
    METHOD() void one(), two(int);
};
