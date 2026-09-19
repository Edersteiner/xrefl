#include <cstdio>

#include "game.xrefl.all.h"

static void dump(const char* type, const char* const* names, int count) {
    std::printf("%s:", type);
    for (int i = 0; i < count; ++i) {
        std::printf(" %s", names[i]);
    }
    std::printf("\n");
}

int main() {
    int count = 0;
    const char* const* names = field_names_game_Door(&count);
    dump("game::Door", names, count);
    return 0;
}
