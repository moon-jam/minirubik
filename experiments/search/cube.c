#define main baseline_main
#include "../baseline/solver.c"
#undef main
#include "cube.h"

void cube_build_transitions(uint16_t pt[3][CUBE_PERMUTATIONS],
                            uint16_t ot[3][CUBE_ORIENTATIONS])
{
    state_t s, t;
    for (unsigned p = 0; p < PERMUTATIONS; p++) {
        unrank_state(p * 729, &s);
        for (unsigned f = 0; f < 3; f++) {
            t = quarter_turn(s, f);
            pt[f][p] = rank_state(&t) / 729;
        }
    }
    for (unsigned o = 0; o < ORIENTATIONS; o++) {
        unrank_state(o, &s);
        for (unsigned f = 0; f < 3; f++) {
            t = quarter_turn(s, f);
            ot[f][o] = rank_state(&t) % 729;
        }
    }
}
void cube_unrank_permutation(uint16_t rank, uint8_t p[CUBE_CUBIES])
{
    state_t s;
    unrank_state((uint32_t) rank * ORIENTATIONS, &s);
    memcpy(p, s.p, CUBIES);
}
uint16_t cube_rank_permutation(const uint8_t p[CUBE_CUBIES])
{
    state_t s = {0};
    memcpy(s.p, p, CUBIES);
    return (uint16_t) (rank_state(&s) / ORIENTATIONS);
}
int cube_parse_rank(const char *input, uint32_t *rank)
{
    state_t s;
    if (!parse_state(input, &s))
        return 0;
    *rank = rank_state(&s);
    return 1;
}
uint32_t cube_inverse_rank(uint16_t p, uint16_t o)
{
    state_t s, t = {0};
    unrank_state((uint32_t) p * ORIENTATIONS + o, &s);
    for (unsigned i = 0; i < CUBIES; ++i) {
        t.p[s.p[i]] = (uint8_t) i;
        t.o[s.p[i]] = (uint8_t) ((3 - s.o[i]) % 3);
    }
    return rank_state(&t);
}
