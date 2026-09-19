#include <cstdio>

#include "registry.xrefl.all.h"

int main() {
    const xrefl::Registry& registry = xrefl_registry();
    std::printf("%d types\n", registry.count);

    for (int i = 0; i < registry.count; ++i) {
        const xrefl::TypeInfo& info = *registry.types[i];
        std::printf("%s (size=%zu, bases=%d)\n", info.name, info.size, info.base_count);
        for (int f = 0; f < info.field_count; ++f) {
            const xrefl::FieldInfo& field = info.fields[f];
            std::printf("    %-12s %-16s offset=%2zu size=%zu\n", field.name, field.type,
                        field.offset, field.size);
        }
    }

    const xrefl::TypeInfo* door = registry.find("game::Door");
    const xrefl::TypeInfo* entity = registry.find("game::Entity");
    const xrefl::TypeInfo* portal = registry.find("game::world::Portal");
    std::printf("Door derives from Entity: %d\n", door->derives_from(*entity));
    std::printf("Portal derives from Entity: %d\n", portal->derives_from(*entity));
    std::printf("Entity derives from Door: %d\n", entity->derives_from(*door));

    game::Door instance{};
    instance.speed = 12.5f;
    const xrefl::FieldInfo* speed = door->find_field("speed");
    std::printf("read back speed = %.1f\n", *speed->get<game::Door, float>(&instance));
    std::printf("wrong type gives null: %d\n", speed->get<game::Door, int>(&instance) == nullptr);
    return 0;
}
