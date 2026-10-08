#include "../variants.h"
#include "../tables.h"

static uint8_t bound(uint16_t p,
                     uint16_t o,
                     unsigned known_half,
                     uint32_t *entry,
                     search_stats_t *stats)
{
    (void) known_half;
    return bounds_merged(p, o, 2, entry, stats);
}

const search_variant_t variant_half_merge2 = {
    .id = 6,
    .name = "Half-turn entries merged in pairs",
    .bound = bound,
    .fixed_table_bytes = sizeof pc + sizeof pd + sizeof hp2,
    .perimeter_radius = 0,
    .inverse_lookup = false,
    .two_bit_half = false,
};
