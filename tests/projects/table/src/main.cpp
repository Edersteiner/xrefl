#include <cstdio>

#include "table.xrefl.all.h"
#include "table.xrefl.all.h"

int main() {
    int count = 0;
    const xrefl::TypeInfo* const* types = xrefl_all_types(&count);
    static_assert(xrefl_type_count == 2);
    std::printf("%d reflected types\n", count);
    for (int i = 0; i < count; ++i) {
        const xrefl::TypeInfo& info = *types[i];
        std::printf("%s (category=\"%s\")\n", info.name, info.category);
        for (int f = 0; f < info.field_count; ++f) {
            std::printf("    %-12s offset=%zu size=%zu%s\n", info.fields[f].name,
                        info.fields[f].offset, info.fields[f].size,
                        info.fields[f].transient ? " transient" : "");
        }
    }
    return 0;
}
