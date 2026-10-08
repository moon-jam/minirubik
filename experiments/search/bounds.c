#include "bounds.h"
#include "tables.h"
#include <assert.h>
uint32_t bounds_find_end(uint32_t r, unsigned radius, search_stats_t *stats)
{
    unsigned ix = radius - 4;
    assert(ix < 2);
    stats->binary_queries++;
    unsigned lo = 0, hi = end_n[ix];
    while (lo < hi) {
        unsigned mid = (lo + hi) / 2;
        if ((ends[ix][mid] >> 7) < r)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo < end_n[ix] && (ends[ix][lo] >> 7) == r ? ends[ix][lo]
                                                      : UINT32_MAX;
}
uint8_t bounds_separate(uint16_t p,
                        uint16_t o,
                        unsigned known_half,
                        uint32_t *entry,
                        search_stats_t *stats)
{
    (void) known_half;
    stats->lookups++;
    *entry = UINT32_MAX;
    uint8_t a = nib(pd, p), b = nib(od, o);
    return a > b ? a : b;
}
uint8_t bounds_half4(uint16_t p,
                     uint16_t o,
                     unsigned known_half,
                     uint32_t *entry,
                     search_stats_t *stats)
{
    (void) known_half;
    stats->lookups++;
    *entry = UINT32_MAX;
    COUNT_OPERATION(stats, permutation_distances);
    COUNT_OPERATION(stats, class_reads);
    COUNT_OPERATION(stats, half_distances);
    uint8_t a = nib(pd, p), b = nib(hp, (uint32_t) pc[p] * 729 + o);
    return a > b ? a : b;
}
uint8_t bounds_merged(uint16_t p,
                      uint16_t o,
                      unsigned block,
                      uint32_t *entry,
                      search_stats_t *stats)
{
    assert(block == 2 || block == 4);
    stats->lookups++;
    *entry = UINT32_MAX;
    unsigned key = (uint32_t) pc[p] * 729 + o;
    uint8_t a = nib(pd, p), b = nib(block == 2 ? hp2 : hp4, key / block);
    return a > b ? a : b;
}
uint8_t bounds_add_perimeter(uint8_t h,
                             uint16_t p,
                             uint16_t o,
                             unsigned radius,
                             uint32_t *entry,
                             search_stats_t *stats)
{
    if (h <= radius) {
        *entry = bounds_find_end((uint32_t) p * 729 + o, radius, stats);
        uint8_t v = *entry == UINT32_MAX ? radius + 1 : (*entry & 7);
        if (v > h)
            h = v;
    }
    return h;
}
unsigned bounds_mod_half(uint16_t p, uint16_t o, search_stats_t *stats)
{
    COUNT_OPERATION(stats, class_reads);
    COUNT_OPERATION(stats, half_residues);
    unsigned key = (uint32_t) pc[p] * 729 + o;
    return (hm[key / 4] >> ((key % 4) * 2)) & 3;
}
unsigned bounds_root_half(uint16_t p, uint16_t o, search_stats_t *stats)
{
    unsigned steps = 0;
    for (;;) {
        COUNT_OPERATION(stats, class_reads);
        if (pc[p] == 0 && o == 0)
            break;
        unsigned m = bounds_mod_half(p, o, stats), want = m == 0 ? 2 : m - 1;
        int found = 0;
        for (unsigned f = 0; f < 3 && !found; f++) {
            uint16_t np = p, no = o;
            for (unsigned k = 0; k < 3; k++) {
                COUNT_OPERATION(stats, permutation_transitions);
                COUNT_OPERATION(stats, orientation_transitions);
                np = pt[f][np];
                no = ot[f][no];
                if (bounds_mod_half(np, no, stats) == want) {
                    p = np;
                    o = no;
                    found = 1;
                    break;
                }
            }
        }
        assert(found && ++steps <= 11);
        COUNT_OPERATION(stats, descent_steps);
    }
    return steps;
}
