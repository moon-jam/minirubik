/* Native comparison runner. Search timing excludes table preparation.
 * table_bytes describes the selected tables, not the process footprint.
 * generated counts child attempts, including pruned and repeated states.
 * These experiments use allocation and recursion; they are not RV32I code.
 */
#define _POSIX_C_SOURCE 200809L
#include "ida_star.h"
#include "tables.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef NDEBUG
#error Search experiments require assertions; do not define NDEBUG.
#endif
static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}
static int cmp(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *) a, y = *(const uint64_t *) b;
    return x < y ? -1 : x > y;
}
int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--list")) {
        variant_print_list();
        return 0;
    }
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "usage: %s MODE(0..7) [COUNT(1..2644)|all]\n", argv[0]);
        return 2;
    }
    char *end;
    unsigned long parsed = strtoul(argv[1], &end, 10);
    if (end == argv[1] || *end || parsed > 7)
        return 2;
    unsigned mode = (unsigned) parsed;
    const search_variant_t *variant = variant_get(mode);
    assert(variant);
    search_result_t outcome;
    int all = argc == 3 && !strcmp(argv[2], "all");
    unsigned limit = 2644;
    if (argc == 3 && !all) {
        parsed = strtoul(argv[2], &end, 10);
        if (end == argv[2] || *end || parsed < 1 || parsed > 2644)
            return 2;
        limit = (unsigned) parsed;
    }
    tables_prepare(variant->two_bit_half);
    if (variant->inverse_lookup)
        for (unsigned r = 0; r < STATES; r++) {
            unsigned ir = cube_inverse_rank(r / 729, r % 729);
            assert(exact[ir] == exact[r] &&
                   cube_inverse_rank(ir / 729, ir % 729) == r);
        }
    if (all) {
        double start = now();
        uint64_t worst = 0;
        for (unsigned r = 0; r < STATES; r++) {
            assert(ida_solve(variant, r, &outcome) == exact[r]);
            if (outcome.stats.generated > worst)
                worst = outcome.stats.generated;
        }
        printf(
            "{\"mode\":%u,\"all_states_verified\":%u,\"seconds\":%.6f,\"max_"
            "generated\":%llu}\n",
            mode, STATES, now() - start, (unsigned long long) worst);
        tables_free();
        return 0;
    }
    assert(ida_solve(variant, 0, &outcome) == 0);
    assert(ida_solve(variant, tables_next(0, 0), &outcome) == 1);
    assert(ida_solve(variant, tables_next(tables_next(0, 0), 6), &outcome) ==
           exact[tables_next(tables_next(0, 0), 6)]);
    FILE *vectors = fopen("baseline/solutions.txt", "r");
    assert(vectors);
    char line[256];
    while (fgets(line, sizeof line, vectors)) {
        if (line[0] == '#' || line[0] == '\n')
            continue;
        char *bar = strchr(line, '|');
        assert(bar);
        *bar = 0;
        uint32_t r;
        assert(cube_parse_rank(line, &r));
        assert(ida_solve(variant, r, &outcome) == exact[r]);
    }
    fclose(vectors);
    uint32_t hard[2644];
    unsigned hn = 0;
    for (unsigned r = 0; r < STATES; r++)
        if (exact[r] == 11)
            hard[hn++] = r;
    assert(hn == 2644 && limit <= hn);
    uint64_t counts[2644], esum = 0, gsum = 0, lsum = 0, bsum = 0;
    unsigned capped = 0;
    double start = now();
    for (unsigned i = 0; i < limit; i++) {
        unsigned index = (unsigned) ((uint64_t) i * hn / limit);
        int result = ida_solve(variant, hard[index], &outcome);
        if (result < 0)
            capped++;
        counts[i] = outcome.stats.generated;
        esum += outcome.stats.expanded;
        gsum += outcome.stats.generated;
        lsum += outcome.stats.lookups;
        bsum += outcome.stats.binary_queries;
    }
    double seconds = now() - start;
    qsort(counts, limit, sizeof *counts, cmp);
    uint32_t designated;
    assert(cube_parse_rank("21345671111111", &designated));
    int result = ida_solve(variant, designated, &outcome);
    size_t bytes = sizeof pt + sizeof ot + variant->fixed_table_bytes;
    if (variant->perimeter_radius)
        bytes += (size_t) end_n[variant->perimeter_radius - 4] * 4;
    printf(
        "{\"mode\":%u,\"inputs\":%u,\"capped\":%u,\"cap\":%llu,\"table_bytes\":"
        "%zu,\"seconds\":%.6f,\"mean_generated\":%.2f,\"p95_generated\":%llu,"
        "\"max_generated\":%llu,\"mean_expanded\":%.2f,\"mean_heuristic_"
        "calls\":%.2f,\"mean_binary_queries\":%.2f,\"sample_length\":%d,"
        "\"sample_generated\":%llu}\n",
        mode, limit, capped, SEARCH_CAP, bytes, seconds, (double) gsum / limit,
        counts[(limit - 1) * 95 / 100], counts[limit - 1],
        (double) esum / limit, (double) lsum / limit, (double) bsum / limit,
        result, (unsigned long long) outcome.stats.generated);
    tables_free();
    return capped || result < 0 ? 1 : 0;
}
