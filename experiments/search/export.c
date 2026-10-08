/* Export only the five retained tables for the C solver. */
#include "tables.h"
#include <stdio.h>

static void emit_transitions(const char *name, const uint16_t *rows[3],
                             unsigned width)
{
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
