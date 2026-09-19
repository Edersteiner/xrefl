namespace game {

REFLECT()
struct Multi : public Entity, protected ::ns::Base<int>, virtual private a::b::Other {
    PROPERTY() int x;
};

REFLECT()
class Defaulted : Entity, virtual Renderable {
    PROPERTY() int hidden;
public:
    PROPERTY() int shown;
protected:
    PROPERTY() int guarded;
};

}
