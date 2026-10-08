#include "variants.h"
#include <stdio.h>
static const search_variant_t *const variants[VARIANT_COUNT] = {
    &variant_separate,         &variant_half4,         &variant_perimeter5,
    &variant_half4_perimeter4, &variant_half4_inverse, &variant_half_merge4,
    &variant_half_merge2,      &variant_half_mod3,
};
const search_variant_t *variant_get(unsigned id)
{
    return id < VARIANT_COUNT ? variants[id] : NULL;
}
void variant_print_list(void)
{
    for (unsigned i = 0; i < VARIANT_COUNT; ++i)
        printf("%u: %s\n", variants[i]->id, variants[i]->name);
}
