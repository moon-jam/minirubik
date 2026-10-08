#ifndef SEARCH_CUBE_H
#define SEARCH_CUBE_H
#include <stdint.h>
enum {
    CUBE_CUBIES = 7,
    CUBE_PERMUTATIONS = 5040,
    CUBE_ORIENTATIONS = 729,
    CUBE_STATES = CUBE_PERMUTATIONS * CUBE_ORIENTATIONS
};
/* The baseline adapter is the only module that includes the frozen solver. */
void cube_build_transitions(uint16_t pt[3][CUBE_PERMUTATIONS],
                            uint16_t ot[3][CUBE_ORIENTATIONS]);
void cube_unrank_permutation(uint16_t rank, uint8_t p[CUBE_CUBIES]);
uint16_t cube_rank_permutation(const uint8_t p[CUBE_CUBIES]);
int cube_parse_rank(const char *input, uint32_t *rank);
uint32_t cube_inverse_rank(uint16_t p, uint16_t o);
#endif
