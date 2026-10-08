#ifndef SEARCH_BOUNDS_H
#define SEARCH_BOUNDS_H
#include <stdint.h>
#ifdef PROFILE_OPERATIONS
/* Logical entry accesses, not machine instructions or cache misses. */
typedef struct {
    uint64_t permutation_transitions, orientation_transitions;
    uint64_t class_reads, permutation_distances, half_distances, half_residues;
    uint64_t distance_updates, descent_steps;
} operation_stats_t;
#endif
typedef struct {
    uint64_t generated, expanded, lookups, binary_queries;
#ifdef PROFILE_OPERATIONS
    operation_stats_t setup, search;
    unsigned searching;
#endif
} search_stats_t;
#ifdef PROFILE_OPERATIONS
#define COUNT_OPERATION(stats, field)                                        \
    do {                                                                     \
        search_stats_t *counted_stats = (stats);                              \
        if (counted_stats) {                                                 \
            operation_stats_t *ops =                                         \
                counted_stats->searching ? &counted_stats->search             \
                                         : &counted_stats->setup;             \
            ops->field++;                                                    \
        }                                                                    \
    } while (0)
#else
#define COUNT_OPERATION(stats, field) ((void) (stats))
#endif
static inline uint8_t nib(const uint8_t *t, uint32_t i)
{
    return (uint8_t) ((t[i >> 1] >> ((i & 1) * 4)) & 15);
}
uint8_t bounds_separate(uint16_t p,
                        uint16_t o,
                        unsigned known_half,
                        uint32_t *entry,
                        search_stats_t *stats);
uint8_t bounds_half4(uint16_t p,
                     uint16_t o,
                     unsigned known_half,
                     uint32_t *entry,
                     search_stats_t *stats);
uint8_t bounds_merged(uint16_t p,
                      uint16_t o,
                      unsigned block,
                      uint32_t *entry,
                      search_stats_t *stats);
uint8_t bounds_add_perimeter(uint8_t h,
                             uint16_t p,
                             uint16_t o,
                             unsigned radius,
                             uint32_t *entry,
                             search_stats_t *stats);
uint32_t bounds_find_end(uint32_t rank, unsigned radius, search_stats_t *stats);
unsigned bounds_mod_half(uint16_t p, uint16_t o, search_stats_t *stats);
unsigned bounds_root_half(uint16_t p, uint16_t o, search_stats_t *stats);
#endif
