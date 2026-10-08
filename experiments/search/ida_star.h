#ifndef SEARCH_IDA_STAR_H
#define SEARCH_IDA_STAR_H
#include "variants.h"
#ifndef SEARCH_CAP
#define SEARCH_CAP 1000000ULL
#endif
enum { SEARCH_PATH_CAPACITY = 12 };
typedef struct {
    int length;
    uint8_t moves[SEARCH_PATH_CAPACITY];
    search_stats_t stats;
} search_result_t;
typedef struct {
    uint16_t p, o;
    unsigned half_distance, first_bound;
} search_seed_t;
/* Separate phases for batched profiling; verification stays outside timing. */
void ida_prepare(const search_variant_t *variant, uint32_t rank,
                 search_seed_t *seed, search_result_t *result);
int ida_search_prepared(const search_variant_t *variant,
                       const search_seed_t *seed, search_result_t *result);
void ida_verify(uint32_t rank, const search_result_t *result);
/* Native recursive IDA*. Target code will use fixed search frames. */
int ida_solve(const search_variant_t *variant,
              uint32_t rank,
              search_result_t *result);
#endif
