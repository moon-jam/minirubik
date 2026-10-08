#include "../variants.h"
#include "../tables.h"

const search_variant_t variant_half4 = {
    .id = 1,
    .name = "Four-bit half-turn subgroup",
    .bound = bounds_half4,
    .fixed_table_bytes = sizeof pc + sizeof pd + sizeof hp,
    .perimeter_radius = 0,
    .inverse_lookup = false,
    .two_bit_half = false,
};
