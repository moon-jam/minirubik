#include "../variants.h"
#include "../tables.h"

static uint8_t bound(uint16_t p,
                     uint16_t o,
                     unsigned known_half,
                     uint32_t *entry,
                     search_stats_t *stats)
{
    uint8_t h = bounds_separate(p, o, known_half, entry, stats);
    return bounds_add_perimeter(h, p, o, 5, entry, stats);
}

const search_variant_t variant_perimeter5 = {
    .id = 2,
    .name = "Separate bounds with radius-five perimeter",
    .bound = bound,
    .fixed_table_bytes = sizeof pd + sizeof od,
    .perimeter_radius = 5,
    .inverse_lookup = false,
    .two_bit_half = false,
};
