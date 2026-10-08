#ifndef SEARCH_VARIANTS_H
#define SEARCH_VARIANTS_H
#include "bounds.h"
#include <stdbool.h>
#include <stddef.h>
typedef uint8_t (*bound_fn)(uint16_t p,
                            uint16_t o,
                            unsigned known_half,
                            uint32_t *entry,
                            search_stats_t *stats);
/* Each variant file owns its bound and the features it adds to IDA*. */
typedef struct {
    unsigned id;
    const char *name;
    bound_fn bound;
    size_t fixed_table_bytes;
    unsigned perimeter_radius;
    bool inverse_lookup;
    bool two_bit_half;
} search_variant_t;
enum { VARIANT_COUNT = 8 };
extern const search_variant_t variant_separate;
extern const search_variant_t variant_half4;
extern const search_variant_t variant_perimeter5;
extern const search_variant_t variant_half4_perimeter4;
extern const search_variant_t variant_half4_inverse;
extern const search_variant_t variant_half_merge4;
extern const search_variant_t variant_half_merge2;
extern const search_variant_t variant_half_mod3;
const search_variant_t *variant_get(unsigned id);
void variant_print_list(void);
#endif
