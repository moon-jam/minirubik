#include "../variants.h"
#include "../tables.h"

static uint8_t bound(uint16_t p,
                     uint16_t o,
                     unsigned known_half,
                     uint32_t *entry,
                     search_stats_t *stats)
{
    (void) o;
    stats->lookups++;
    *entry = UINT32_MAX;
    COUNT_OPERATION(stats, permutation_distances);
    uint8_t h = nib(pd, p);
    return known_half > h ? (uint8_t) known_half : h;
}

const search_variant_t variant_half_mod3 = {
    .id = 7,
    .name = "Two-bit exact half-turn distances",
    .bound = bound,
    .fixed_table_bytes = sizeof pc + sizeof pd + sizeof hm,
    .perimeter_radius = 0,
    .inverse_lookup = false,
    .two_bit_half = true,
};
