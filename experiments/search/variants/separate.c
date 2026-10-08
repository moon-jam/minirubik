#include "../variants.h"
#include "../tables.h"

const search_variant_t variant_separate = {
    .id = 0,
    .name = "Separate permutation/orientation",
    .bound = bounds_separate,
    .fixed_table_bytes = sizeof pd + sizeof od,
    .perimeter_radius = 0,
    .inverse_lookup = false,
    .two_bit_half = false,
};
