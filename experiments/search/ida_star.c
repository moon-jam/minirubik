#include "ida_star.h"
#include "tables.h"
#include <assert.h>
#include <string.h>
#ifdef NDEBUG
#error Search experiments require assertions; do not define NDEBUG.
#endif
static int dfs(const search_variant_t *variant,
               search_result_t *result,
               uint16_t p,
               uint16_t o,
               unsigned depth,
               unsigned last,
               unsigned bound,
               unsigned gh)
{
    uint32_t entry = UINT32_MAX;
    unsigned h = variant->bound(p, o, gh, &entry, &result->stats);
    if (depth + h > bound)
        return 0;
    if (variant->inverse_lookup) {
        uint32_t ir = cube_inverse_rank(p, o), unused;
        unsigned ih =
            variant->bound(ir / 729, ir % 729, 0, &unused, &result->stats);
        if (depth + ih > bound)
            return 0;
    }
    if (p == 0 && o == 0)
        return (int) depth + 1;
    if (entry != UINT32_MAX) {
        unsigned radius = variant->perimeter_radius;
        while (entry & 7) {
            unsigned m = (entry >> 3) & 15;
            assert(depth < 11);
            result->moves[depth++] = m;
            uint32_t r = tables_next((uint32_t) p * 729 + o, m);
            p = r / 729;
            o = r % 729;
            entry = bounds_find_end(r, radius, &result->stats);
            assert(entry != UINT32_MAX);
        }
        assert(!p && !o);
        return (int) depth + 1;
    }
    if (depth == bound)
        return 0;
    result->stats.expanded++;
    for (unsigned f = 0; f < 3; f++)
        if (f != last) {
            uint16_t np = p, no = o;
            unsigned nh = gh,
                     oldmod = variant->two_bit_half
                                  ? bounds_mod_half(p, o, &result->stats) : 0;
            for (unsigned k = 0; k < 3; k++) {
                COUNT_OPERATION(&result->stats, permutation_transitions);
                COUNT_OPERATION(&result->stats, orientation_transitions);
                np = pt[f][np];
                no = ot[f][no];
                if (++result->stats.generated > SEARCH_CAP)
                    return -1;
                result->moves[depth] = f * 3 + k;
                if (variant->two_bit_half) {
                    unsigned newmod = bounds_mod_half(np, no, &result->stats);
                    COUNT_OPERATION(&result->stats, distance_updates);
                    int delta = (int) newmod - (int) oldmod;
                    if (delta == -2)
                        delta = 1;
                    else if (delta == 2)
                        delta = -1;
                    nh = (unsigned) ((int) nh + delta);
                    oldmod = newmod;
                }
                int child_result =
                    dfs(variant, result, np, no, depth + 1, f, bound, nh);
                if (child_result)
                    return child_result;
            }
        }
    return 0;
}
void ida_prepare(const search_variant_t *variant, uint32_t r,
                 search_seed_t *seed, search_result_t *result)
{
    memset(result, 0, sizeof *result);
    result->length = -1;
    uint16_t p = r / 729, o = r % 729;
    uint32_t entry;
    unsigned gh = variant->two_bit_half
                      ? bounds_root_half(p, o, &result->stats) : 0;
    if (variant->two_bit_half)
        COUNT_OPERATION(&result->stats, permutation_distances);
    unsigned lower = variant->two_bit_half
                         ? nib(pd, p)
                         : variant->bound(p, o, gh, &entry, &result->stats);
    if (gh > lower)
        lower = gh;
    *seed = (search_seed_t) {p, o, gh, lower};
}
int ida_search_prepared(const search_variant_t *variant,
                       const search_seed_t *seed, search_result_t *result)
{
#ifdef PROFILE_OPERATIONS
    result->stats.searching = 1;
#endif
    for (unsigned bound = seed->first_bound; bound <= 11; ++bound) {
        int v = dfs(variant, result, seed->p, seed->o, 0, 3, bound,
                    seed->half_distance);
        if (v < 0)
            return -1;
        if (v > 0) {
            result->length = v - 1;
            return result->length;
        }
    }
    assert(0);
    return -1;
}
void ida_verify(uint32_t r, const search_result_t *result)
{
    assert(result->length == exact[r]);
    unsigned n = r;
    for (int i = 0; i < result->length; ++i)
        n = tables_next(n, result->moves[i]);
    assert(n == 0);
}
int ida_solve(const search_variant_t *variant, uint32_t r,
              search_result_t *result)
{
    search_seed_t seed;
    ida_prepare(variant, r, &seed, result);
    int length = ida_search_prepared(variant, &seed, result);
    if (length >= 0)
        ida_verify(r, result);
    return length;
}
