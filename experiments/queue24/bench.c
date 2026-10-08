#define _POSIX_C_SOURCE 200809L
#define main solver_cli_main
#include "solver.c"
#undef main
#include <time.h>

static double elapsed_ms(struct timespec start, struct timespec end)
{
    return (end.tv_sec - start.tv_sec) * 1000.0 +
           (end.tv_nsec - start.tv_nsec) / 1000000.0;
}

int main(int argc, char **argv)
{
    const int export_table = argc == 2 && !strcmp(argv[1], "--export");
    const int benchmark = argc == 2 && !strcmp(argv[1], "--benchmark");
    if (!export_table && !benchmark)
        return 2;
    if (export_table && !self_test())
        return 1;
    struct timespec start, end;
    if (clock_gettime(CLOCK_MONOTONIC, &start))
        return 1;
    uint8_t diameter;
    uint8_t *table = build_table(&diameter);
    if (clock_gettime(CLOCK_MONOTONIC, &end) || !table || diameter != 11) {
        free(table);
        return 1;
    }
    if (export_table) {
        const int ok = fwrite(table, 1, STATES, stdout) == STATES;
        free(table);
        return !ok || output_failed();
    }
    struct natural_fields { uint16_t low; uint8_t high; };
    printf("{\"layout\":%d,\"state_layout\":%d,\"state_bytes\":%zu,\"entry_bytes\":%zu,\"queue_bytes\":%zu,"
           "\"natural_fields_bytes\":%zu,\"build_ms\":%.6f}\n",
           QUEUE_LAYOUT, STATE_LAYOUT, sizeof(state_t), sizeof(queue_entry_t),
           (size_t)STATES * sizeof(queue_entry_t), sizeof(struct natural_fields),
           elapsed_ms(start, end));
    free(table);
    return output_failed();
}
