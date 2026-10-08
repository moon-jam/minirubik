#include "../variants.h"
#include "../tables.h"

static uint8_t bound(uint16_t p,
                     uint16_t o,
                     unsigned known_half,
                     uint32_t *entry,
                     search_stats_t *stats)
{
    uint8_t h = bounds_half4(p, o, known_half, entry, stats);
    return bounds_add_perimeter(h, p, o, 4, entry, stats);
}

const search_variant_t variant_half4_perimeter4 = {
    .id = 3,
    .name = "Half-turn bound with radius-four perimeter",
    .bound = bound,
    .fixed_table_bytes = sizeof pc + sizeof pd + sizeof hp,
    .perimeter_radius = 4,
    .inverse_lookup = false,
    .two_bit_half = false,
};
