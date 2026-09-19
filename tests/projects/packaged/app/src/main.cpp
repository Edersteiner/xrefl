#include <cstdio>

#include "app.xrefl.all.h"

int main() {
    const xrefl::Registry& registry = xrefl_registry();
    std::printf("%d types from the installed package\n", registry.count);
    const xrefl::TypeInfo* chunk = registry.find("game::Chunk");
    for (int i = 0; i < chunk->field_count; ++i) {
        std::printf("    %s %s at %zu\n", chunk->fields[i].type, chunk->fields[i].name,
                    chunk->fields[i].offset);
    }
    return 0;
}
