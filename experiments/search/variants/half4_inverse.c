#include "../variants.h"
#include "../tables.h"

const search_variant_t variant_half4_inverse = {
    .id = 4,
    .name = "Half-turn bound with inverse lookup",
    .bound = bounds_half4,
    .fixed_table_bytes = sizeof pc + sizeof pd + sizeof hp,
    .perimeter_radius = 0,
    .inverse_lookup = true,
    .two_bit_half = false,
};
