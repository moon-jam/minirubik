/* Focused native profiling of the four-bit and two-bit exact heuristics.
 * Count and timing builds are separate. Neither reports RV32I instructions.
 */
#define _POSIX_C_SOURCE 200809L
#include "ida_star.h"
#include "tables.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef PROFILE_OPERATIONS
#define BATCHES 1
#define OP_FIELDS(X)                                                          \
    X(permutation_transitions) X(orientation_transitions) X(class_reads)       \
    X(permutation_distances) X(half_distances) X(half_residues)                 \
    X(distance_updates) X(descent_steps)
static void add_ops(operation_stats_t *sum, const operation_stats_t *item)
{
#define ADD(field) sum->field += item->field;
    OP_FIELDS(ADD)
#undef ADD
}
static void print_ops(const operation_stats_t *ops)
{
    const char *sep = "";
    putchar('{');
#define PRINT(field)                                                          \
    printf("%s\"" #field "\":%llu", sep, (unsigned long long) ops->field);     \
    sep = ",";
    OP_FIELDS(PRINT)
#undef PRINT
    putchar('}');
}
#else
#define BATCHES 32
static double now(void)
{
    struct timespec t;
    assert(clock_gettime(CLOCK_MONOTONIC, &t) == 0);
    return t.tv_sec + t.tv_nsec / 1e9;
}
#endif

int main(int argc, char **argv)
{
    if (argc != 2 || (strcmp(argv[1], "1") && strcmp(argv[1], "7"))) {
        fprintf(stderr, "usage: %s MODE(1|7)\n", argv[0]);
        return 2;
    }
    const search_variant_t *variant = variant_get((unsigned) atoi(argv[1]));
    tables_prepare(variant->two_bit_half);
    uint32_t ranks[2644];
    unsigned n = 0;
    for (unsigned r = 0; r < STATES; r++)
        if (exact[r] == 11) {
            assert(n < 2644);
            ranks[n++] = r;
        }
    assert(n == 2644);
    search_seed_t *seeds = calloc(n, sizeof *seeds);
    search_result_t *results = calloc(n, sizeof *results);
    assert(seeds && results);

#ifndef PROFILE_OPERATIONS
    /* Untimed warm-up before measuring repeated batches. */
    for (unsigned i = 0; i < n; i++)
        assert(ida_solve(variant, ranks[i], &results[i]) == 11);
    double setup_seconds = 0, search_seconds = 0;
#endif
    for (unsigned batch = 0; batch < BATCHES; batch++) {
#ifndef PROFILE_OPERATIONS
        double start = now();
#endif
        for (unsigned i = 0; i < n; i++)
            ida_prepare(variant, ranks[i], &seeds[i], &results[i]);
#ifndef PROFILE_OPERATIONS
        setup_seconds += now() - start;
#endif
    }

    uint64_t attempts = 0, expanded = 0;
#ifdef PROFILE_OPERATIONS
    operation_stats_t setup = {0}, search = {0};
#endif
    for (unsigned batch = 0; batch < BATCHES; batch++) {
        /* Reset search counters outside timing; preserve prepared roots. */
        for (unsigned i = 0; i < n; i++) {
            results[i].stats.generated = results[i].stats.expanded = 0;
            results[i].stats.lookups = results[i].stats.binary_queries = 0;
        }
#ifndef PROFILE_OPERATIONS
        double start = now();
#endif
        for (unsigned i = 0; i < n; i++) {
            int length = ida_search_prepared(variant, &seeds[i], &results[i]);
            assert(length == 11);
        }
#ifndef PROFILE_OPERATIONS
        search_seconds += now() - start;
#endif
        /* Check every answer outside the timed and counted phases. */
        for (unsigned i = 0; i < n; i++) {
            ida_verify(ranks[i], &results[i]);
            if (batch == 0) {
                attempts += results[i].stats.generated;
                expanded += results[i].stats.expanded;
            }
#ifdef PROFILE_OPERATIONS
            const search_stats_t *stats = &results[i].stats;
            assert(stats->search.permutation_transitions == stats->generated);
            assert(stats->search.orientation_transitions == stats->generated);
            assert(stats->search.distance_updates ==
                   (variant->two_bit_half ? stats->generated : 0));
            assert(stats->setup.descent_steps ==
                   (variant->two_bit_half ? seeds[i].half_distance : 0));
            add_ops(&setup, &stats->setup);
            add_ops(&search, &stats->search);
#endif
        }
    }
    printf("{\"mode\":%u,\"inputs\":%u,\"batches\":%u,"
           "\"attempts_per_batch\":%llu,\"expanded_per_batch\":%llu,",
           variant->id, n, BATCHES, (unsigned long long) attempts,
           (unsigned long long) expanded);
#ifdef PROFILE_OPERATIONS
    printf("\"measurement\":\"logical_operations\",\"setup\":");
    print_ops(&setup);
    printf(",\"search\":");
    print_ops(&search);
#else
    printf("\"measurement\":\"native_phase_timing\","
           "\"setup_seconds_per_batch\":%.9f,"
           "\"search_seconds_per_batch\":%.9f",
           setup_seconds / BATCHES, search_seconds / BATCHES);
#endif
    puts("}");
    free(results);
    free(seeds);
    tables_free();
    return 0;
}
