namespace outer { namespace inner {

REFLECT()
struct Host {
    PROPERTY() int own;

    REFLECT()
    struct Nested {
        PROPERTY() float value;
    };

    REFLECT()
    enum class Mode { A, B };
};

} }
