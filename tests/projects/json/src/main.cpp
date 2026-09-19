#include <cstdio>

#include "json.xrefl.all.h"

int main() {
    game::Level level;
    level.title = "Atrium";

    game::Door door;
    door.id = 7;
    door.name = "north";
    door.state = game::DoorState::Ajar;
    door.speed = 2.5f;
    door.tint = {1.0f, 0.5f, 0.0f};
    door.tags = {"heavy", "locked"};
    door.animCursor = 0.75f;              // transient, must not appear
    level.doors.push_back(door);

    std::string text = xrefl::json::to_string(level);
    std::printf("%s\n", text.c_str());

    game::Level round_trip;
    bool ok = xrefl::json::from_string(text, round_trip);
    std::printf("read back: %d\n", ok);
    std::printf("title=%s doors=%zu\n", round_trip.title.c_str(), round_trip.doors.size());
    const game::Door& back = round_trip.doors[0];
    std::printf("id=%u name=%s state=%d speed=%.1f tint=(%.1f,%.1f,%.1f) tags=%zu\n",
                back.id, back.name.c_str(), static_cast<int>(back.state), back.speed,
                back.tint.r, back.tint.g, back.tint.b, back.tags.size());
    std::printf("animCursor stayed at its default: %.2f\n", back.animCursor);

    // A file from before `speed` and `tags` existed.
    game::Door legacy;
    legacy.speed = 99.0f;
    ok = xrefl::json::from_string("{\"id\":3,\"name\":\"old\"}", legacy);
    std::printf("old file reads: %d, id=%u, speed kept default %.1f\n", ok, legacy.id,
                legacy.speed);

    // A wrong type is a mismatch, not an old file.
    game::Door broken;
    ok = xrefl::json::from_string("{\"id\":\"not a number\"}", broken);
    std::printf("wrong type rejected: %d\n", !ok);

    std::printf("\n-- polymorphic --\n");
    const xrefl::json::PolymorphicRegistry& registry = xrefl_json_registry();

    shapes::Square square;
    square.name = "tile";
    square.width = 3.0;
    square.height = 3.0;
    square.locked = true;

    const shapes::Shape& as_shape = square;
    std::string saved = xrefl::json::save_polymorphic(registry, as_shape);
    std::printf("%s\n", saved.c_str());

    // The concrete type comes from the document.
    std::unique_ptr<shapes::Shape> loaded =
        xrefl::json::load_polymorphic<shapes::Shape>(registry, saved);
    std::printf("loaded: %d\n", loaded != nullptr);
    if (loaded) {
        shapes::Square* back = dynamic_cast<shapes::Square*>(loaded.get());
        std::printf("concrete type is Square: %d\n", back != nullptr);
        std::printf("name=%s width=%.1f locked=%d\n", loaded->name.c_str(),
                    back ? back->width : 0.0, back ? back->locked : false);
    }

    std::unique_ptr<shapes::Rect> as_rect =
        xrefl::json::load_polymorphic<shapes::Rect>(registry, saved);
    std::printf("loads as Rect too: %d\n", as_rect != nullptr);

    // Refused before construction.
    std::unique_ptr<shapes::Shape> wrong = xrefl::json::load_polymorphic<shapes::Shape>(
        registry, "{\"$type\":\"shapes::Marker\",\"index\":4}");
    std::printf("unrelated type refused: %d\n", wrong == nullptr);

    std::unique_ptr<shapes::Shape> unknown = xrefl::json::load_polymorphic<shapes::Shape>(
        registry, "{\"$type\":\"shapes::Nope\"}");
    std::printf("unknown type refused: %d\n", unknown == nullptr);
    return 0;
}
