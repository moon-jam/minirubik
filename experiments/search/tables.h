#ifndef SEARCH_TABLES_H
#define SEARCH_TABLES_H
#include "cube.h"
#include <stdbool.h>
enum {
    CUBIES = CUBE_CUBIES,
    PERMUTATIONS = CUBE_PERMUTATIONS,
    ORIENTATIONS = CUBE_ORIENTATIONS,
    STATES = CUBE_STATES,
    HC = 210,
    HS = HC * ORIENTATIONS
};
/* Quarter-turn maps: pt[f][p] and ot[f][o], with f = R, B, D. */
extern uint16_t pt[3][PERMUTATIONS], ot[3][ORIENTATIONS];
/* Permutation-to-coset map: 5040 permutations / 24 subgroup elements = 210. */
extern uint8_t pc[PERMUTATIONS];
/* Four-bit distances to solved permutation/orientation, ignoring the other. */
extern uint8_t pd[(PERMUTATIONS + 1) / 2], od[(ORIENTATIONS + 1) / 2];
/* hp: exact coset distances. hp2/hp4: minima over blocks of two/four entries;
 * all three use four bits per stored value. */
extern uint8_t hp[(HS + 1) / 2], hp2[(HS + 3) / 4];
/* hm stores the same exact coset distances modulo three, two bits each. */
extern uint8_t hp4[(HS + 7) / 8], hm[(HS + 3) / 4];
/* Host-only exact oracle and radius-four/radius-five perimeter records. */
extern uint8_t *exact;
extern uint32_t *ends[2];
extern uint32_t end_n[2];
void tables_prepare(bool verify_modulo3);
void tables_free(void);
uint32_t tables_next(uint32_t rank, unsigned move);
#endif
