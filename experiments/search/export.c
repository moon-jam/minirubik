/*
 * Host-only export of the five arrays retained by the C and RV32I solvers:
 *   permutation_transitions[f][p], orientation_transitions[f][o]: next ranks
 *     after one quarter turn of face f (R, B, D).
 *   subgroup_offset[p]: logical distance-table offset for p's class
 *     under the half-turn subgroup H = <R2, B2, D2>.
 *     H partitions the 5,040 permutations into 210 classes of 24.
 *     Each class has 729 orientation entries, so the offset is
 *     class_H(p) * 729.
 *   permutation_distance: exact permutation-only distances, two per byte.
 *   subgroup_remainder: distance to the half-turn subgroup modulo three,
 *     four two-bit entries per byte, indexed by subgroup_offset[p] + o.
 * Construction and verification live in tables_prepare(); the full-state
 * reference distances, temporary queues, and other experiment tables stay
 * on the host and are not emitted here.
 */
#include "tables.h"
#include <stdio.h>

static void emit_transitions(const char *name, const uint16_t *rows[3],
                             unsigned width)
{
    /* All coordinate ranks fit in 16 bits. Half/inverse turns are obtained
     * on the target by applying a quarter-turn row twice/three times. */
    printf("static const uint16_t %s[3][%u] = {\n", name, width);
    for (unsigned f = 0; f < 3; ++f) {
        puts("    {");
        for (unsigned i = 0; i < width; ++i) {
            if (i % 16 == 0) fputs("        ", stdout);
            printf("%u,", rows[f][i]);
            if (i % 16 == 15 || i + 1 == width) putchar('\n');
        }
        puts("    },");
    }
    puts("};");
}

static void emit_bytes(const char *name, const uint8_t *data, unsigned count)
{
    printf("static const uint8_t %s[%u] = {\n", name, count);
    for (unsigned i = 0; i < count; ++i) {
        if (i % 24 == 0) fputs("    ", stdout);
        printf("%u,", data[i]);
        if (i % 24 == 23 || i + 1 == count) putchar('\n');
    }
    puts("};");
}

static void emit_offsets(void)
{
    /* A quotient state is class(p) * 729 + o. Precompute the product for
     * each permutation so RV32I needs only a load and addition at lookup.
     * The largest offset exceeds 16 bits, so these entries use uint32_t. */
    printf("static const uint32_t subgroup_offset[%u] = {\n", PERMUTATIONS);
    for (unsigned i = 0; i < PERMUTATIONS; ++i) {
        if (i % 12 == 0) fputs("    ", stdout);
        printf("%u,", (unsigned) pc[i] * ORIENTATIONS);
        if (i % 12 == 11) putchar('\n');
    }
    puts("};");
}

int main(void)
{
    /* Also verify recovery of full subgroup distances from packed residues
     * before exporting. Only the five selected arrays below are retained. */
    tables_prepare(1);
    puts("/* Generated read-only tables; regenerate with make. */");
    const uint16_t *p[3] = {pt[0], pt[1], pt[2]};
    const uint16_t *o[3] = {ot[0], ot[1], ot[2]};
    emit_transitions("permutation_transitions", p, PERMUTATIONS);
    emit_transitions("orientation_transitions", o, ORIENTATIONS);
    emit_offsets();
    emit_bytes("permutation_distance", pd, sizeof pd);
    emit_bytes("subgroup_remainder", hm, sizeof hm);
    tables_free();
    return fflush(stdout) != 0 || ferror(stdout);
}
