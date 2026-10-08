/* Host-only exhaustive verification against the frozen reference model. */
#define _POSIX_C_SOURCE 200809L
#define main solver_cli_main
#include "../solver.c"
#undef main

#include "../experiments/search/cube.h"
#include <assert.h>
#include <time.h>
void tables_prepare(_Bool verify_modulo3);
void tables_free(void);
extern uint16_t pt[3][PERMUTATIONS], ot[3][ORIENTATIONS];
extern uint8_t pc[PERMUTATIONS], pd[(PERMUTATIONS + 1) / 2];
extern uint8_t hp[(HALF_ENTRIES + 1) / 2], hm[(HALF_ENTRIES + 3) / 4];
extern uint8_t *exact;

static const uint8_t inverse_move[MOVES] = {2, 1, 0, 5, 4, 3, 8, 7, 6};

/*@ requires valid_state(state);
    assigns \nothing;
    ensures \result < STATES;
 */
static uint32_t rank_state(const state_t *state)
{
    coordinate_t c = encode_state(state);
    return (uint32_t) c.p * ORIENTATIONS + c.o;
}

/*@ requires \valid(state); requires rank < STATES; assigns *state; */
static void unrank_state(uint32_t rank, state_t *state)
{
    uint8_t available[CUBIES] = {0, 1, 2, 3, 4, 5, 6};
    uint32_t p = rank / ORIENTATIONS, o = rank % ORIENTATIONS, f = 720;
    uint8_t sum = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t q = (uint8_t) (p / f);
        p %= f;
        state->p[i] = available[q];
        for (uint8_t j = q; j + 1U < CUBIES - i; ++j)
            available[j] = available[j + 1U];
        if (i < 5)
            f /= 6U - i;
    }
    for (uint8_t i = 6; i-- > 0;) {
        state->o[i] = (uint8_t) (o % 3U);
        sum = (uint8_t) (sum + state->o[i]);
        o /= 3U;
    }
    state->o[6] = (uint8_t) ((3U - sum % 3U) % 3U);
}

static int self_test(void)
{
    const state_t solved = {{0, 1, 2, 3, 4, 5, 6}, {0}};
    state_t state;
    for (uint8_t move = 0; move < MOVES; ++move) {
        state = apply_move(apply_move(solved, move), inverse_move[move]);
        if (memcmp(&solved, &state, sizeof solved))
            return 0;
    }
    for (uint32_t rank = 0; rank < STATES; ++rank) {
        unrank_state(rank, &state);
        if (!valid(&state) || rank_state(&state) != rank)
            return 0;
    }
    static const char *const cases[] = {
        "12345671111111", "25346712313322", "21345671111111"
    };
    static const int lengths[] = {0, 2, 11};
    solution_t solution;
    for (unsigned i = 0; i < 3; ++i)
        if (solve_cube(cases[i], &solution) != lengths[i])
            return 0;
    return 1;
}

/* Supplied vectors specify an optimal length, not a unique shortest path. */
static int check_solution(const char *input, const char *text)
{
    state_t state;
    solution_t solution;
    int length = solve_cube(input, &solution);
    if (length < 0 || !parse_state(input, &state))
        return 0;
    unsigned count = 0;
    while (*text) {
        if (*text == ' ') {
            ++text;
            continue;
        }
        unsigned face;
        switch (*text++) {
        case 'R': face = 0; break;
        case 'B': face = 1; break;
        case 'D': face = 2; break;
        default: return 0;
        }
        unsigned turn = 0;
        if (*text == '2') {
            turn = 1;
            ++text;
        } else if (*text == '\'') {
            turn = 2;
            ++text;
        }
        if ((*text && *text != ' ') || ++count > MAX_DEPTH)
            return 0;
        state = apply_move(state, (uint8_t) (face * 3U + turn));
    }
    return count == (unsigned) length && is_solved(&state);
}


static int verify_all(int hard_only)
{
    struct timespec start, finish;
    assert(clock_gettime(CLOCK_MONOTONIC, &start) == 0);
    tables_prepare(1);
    assert(memcmp(permutation_transitions, pt, sizeof pt) == 0);
    assert(memcmp(orientation_transitions, ot, sizeof ot) == 0);
    assert(memcmp(permutation_class, pc, sizeof pc) == 0);
    assert(memcmp(permutation_distance, pd, sizeof pd) == 0);
    assert(memcmp(subgroup_remainder, hm, sizeof hm) == 0);
    /* Independently check the maintained cube operations against the snapshot. */
    state_t state;
    for (unsigned p = 0; p < PERMUTATIONS; ++p) {
        unrank_state(p * ORIENTATIONS, &state);
        for (unsigned f = 0; f < 3; ++f) {
            state_t next = quarter_turn(state, (uint8_t) f);
            assert(encode_state(&next).p == pt[f][p]);
        }
        assert(permutation_bound((uint16_t) p) ==
               ((pd[p >> 1] >> ((p & 1U) * 4U)) & 15U));
    }
    for (unsigned o = 0; o < ORIENTATIONS; ++o) {
        unrank_state(o, &state);
        for (unsigned f = 0; f < 3; ++f) {
            state_t next = quarter_turn(state, (uint8_t) f);
            assert(encode_state(&next).o == ot[f][o]);
        }
    }
    unsigned hmax = 0, pmax = 0, hard = 0, checked = 0;
    uint64_t hard_attempts = 0, hard_expanded = 0, worst_attempts = 0;
    solution_t solution;
    for (unsigned rank = 0; rank < STATES; ++rank) {
        if (hard_only && exact[rank] != MAX_DEPTH)
            continue;
        ++checked;
        uint16_t p = (uint16_t) (rank / ORIENTATIONS);
        uint16_t o = (uint16_t) (rank % ORIENTATIONS);
        unsigned key = (unsigned) pc[p] * ORIENTATIONS + o;
        unsigned h = (hp[key >> 1] >> ((key & 1U) * 4U)) & 15U;
        unsigned dp = permutation_bound(p);
        assert(half_remainder(p, o) == h % 3);
        assert(root_half_distance((coordinate_t) {p, o}) == (int) h);
        assert(h <= exact[rank] && dp <= exact[rank]);
        if (h > hmax) hmax = h;
        if (dp > pmax) pmax = dp;
        unrank_state(rank, &state);
        coordinate_t encoded = encode_state(&state);
        assert(valid(&state) && encoded.p == p && encoded.o == o);
        int length = search(encoded, &solution);
        assert(length == exact[rank]);
        if (exact[rank] == MAX_DEPTH) {
            ++hard;
            hard_attempts += attempts;
            hard_expanded += expanded;
            if (attempts > worst_attempts) worst_attempts = attempts;
        }
        for (unsigned i = 0; i < solution.length; ++i)
            state = apply_move(state, solution.moves[i]);
        assert(is_solved(&state));
        /* Compare the public text interface as well on every hard input. */
        if (exact[rank] == MAX_DEPTH) {
            char input[15];
            unrank_state(rank, &state);
            for (unsigned i = 0; i < CUBIES; ++i) {
                input[i] = (char) ('1' + state.p[i]);
                input[i + CUBIES] = (char) ('1' + state.o[i]);
            }
            input[14] = 0;
            uint32_t reference_rank;
            assert(cube_parse_rank(input, &reference_rank) &&
                   reference_rank == rank);
            assert(solve_cube(input, &solution) == MAX_DEPTH);
        }
    }
    assert(hard == 2644);
    /* Same move order and bound as the selected recursive prototype. */
    assert(hard_attempts == 9567748 && hard_expanded == 1606138 &&
           worst_attempts == 14649);
    assert(solve_cube("21345671111111", &solution) == MAX_DEPTH);
    uint64_t designated_attempts = attempts;
    assert(designated_attempts == 4095);
    assert(clock_gettime(CLOCK_MONOTONIC, &finish) == 0);
    double seconds = finish.tv_sec - start.tv_sec +
                     (finish.tv_nsec - start.tv_nsec) / 1e9;
    printf("{\"states_verified\":%u,\"hard_inputs\":%u,"
           "\"hard_attempts\":%llu,\"max_attempts\":%llu,"
           "\"designated_attempts\":%llu,\"permutation_max\":%u,"
           "\"subgroup_max\":%u,\"wall_seconds\":%.6f}\n",
           checked, hard, (unsigned long long) hard_attempts,
           (unsigned long long) worst_attempts,
           (unsigned long long) designated_attempts, pmax, hmax, seconds);
    tables_free();
    return output_failed();
}

int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--self-test")) {
        if (!self_test()) {
            fputs("self-test failed\n", stderr);
            return 1;
        }
        puts("3674160 rank round trips; solved, short, and distance-11 cases passed");
        return output_failed();
    }
    if (argc == 4 && !strcmp(argv[1], "--check-solution")) {
        if (!check_solution(argv[2], argv[3])) {
            fputs("solution length or replay check failed\n", stderr);
            return 1;
        }
        return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "--verify-hard"))
        return verify_all(1);
    if (argc == 1 || (argc == 2 && !strcmp(argv[1], "--verify-all")))
        return verify_all(0);
    fputs("usage: solver-verify [--verify-all|--verify-hard|--self-test]\n", stderr);
    return 2;
}
