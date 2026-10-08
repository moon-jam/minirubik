#include "tables.h"
#include "bounds.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef NDEBUG
#error Search experiments require assertions; do not define NDEBUG.
#endif
uint16_t pt[3][PERMUTATIONS] = {{0}}, ot[3][ORIENTATIONS] = {{0}};
uint8_t pc[PERMUTATIONS] = {0}, hp[(HS + 1) / 2] = {0};
uint8_t hp2[(HS + 3) / 4] = {0}, hp4[(HS + 7) / 8] = {0},
                       hm[(HS + 3) / 4] = {0};
uint8_t pd[(PERMUTATIONS + 1) / 2] = {0}, od[(ORIENTATIONS + 1) / 2] = {0};
uint8_t *exact;
uint32_t *ends[2];
uint32_t end_n[2];

static void setn(uint8_t *t, uint32_t i, uint8_t v)
{
    unsigned s = (i & 1) * 4;
    t[i >> 1] = (uint8_t) ((t[i >> 1] & ~(15U << s)) | (v << s));
}
uint32_t tables_next(uint32_t r, unsigned move)
{
    uint16_t p = r / 729, o = r % 729;
    for (unsigned k = 0; k <= move % 3; k++) {
        p = pt[move / 3][p];
        o = ot[move / 3][o];
    }
    return (uint32_t) p * 729 + o;
}
static uint8_t *bfs(unsigned total,
                    unsigned width,
                    const uint16_t *a,
                    const uint16_t *b)
{
    uint8_t *d = malloc(total);
    uint32_t *q = malloc((size_t) total * 4);
    assert(d && q);
    memset(d, 255, total);
    d[0] = 0;
    q[0] = 0;
    unsigned h = 0, t = 1;
    while (h < t) {
        uint32_t r = q[h++];
        for (unsigned f = 0; f < 3; f++) {
            unsigned p = r / width, o = r % width;
            for (unsigned k = 0; k < 3; k++) {
                p = a[f * (total / width) + p];
                if (b)
                    o = b[f * width + o];
                unsigned n = p * width + o;
                if (d[n] == 255) {
                    d[n] = d[r] + 1;
                    q[t++] = n;
                }
            }
        }
    }
    assert(t == total);
    free(q);
    return d;
}
void tables_prepare(bool verify_modulo3)
{
    cube_build_transitions(pt, ot);
    exact = bfs(STATES, 729, &pt[0][0], &ot[0][0]);
    unsigned hist[12] = {0};
    for (unsigned r = 0; r < STATES; r++) {
        assert(exact[r] <= 11);
        hist[exact[r]]++;
    }
    assert(hist[11] == 2644 && hist[0] == 1);
    uint8_t *pdist = bfs(PERMUTATIONS, 1, &pt[0][0], NULL);
    uint8_t *odist = bfs(ORIENTATIONS, 1, &ot[0][0], NULL);
    for (unsigned i = 0; i < PERMUTATIONS; i++)
        setn(pd, i, pdist[i]);
    for (unsigned i = 0; i < ORIENTATIONS; i++)
        setn(od, i, odist[i]);
    for (unsigned i = 0; i < PERMUTATIONS; i++)
        assert(nib(pd, i) == pdist[i]);
    for (unsigned i = 0; i < ORIENTATIONS; i++)
        assert(nib(od, i) == odist[i]);
    free(pdist);
    free(odist);
    /* Generate the 24 half-turn elements, all with zero orientation. */
    uint16_t hr[24] = {0};
    unsigned ht = 1;
    for (unsigned h = 0; h < ht; h++)
        for (unsigned f = 0; f < 3; f++) {
            uint16_t n = pt[f][pt[f][hr[h]]];
            unsigned j = 0;
            while (j < ht && hr[j] != n)
                j++;
            if (j == ht) {
                assert(ht < 24);
                hr[ht++] = n;
            }
        }
    assert(ht == 24);
    uint8_t perm[PERMUTATIONS][7];
    for (unsigned p = 0; p < PERMUTATIONS; p++) {
        cube_unrank_permutation((uint16_t) p, perm[p]);
    }
    memset(pc, 255, sizeof pc);
    uint16_t rep[HC];
    unsigned nc = 0;
    for (unsigned p = 0; p < PERMUTATIONS; p++)
        if (pc[p] == 255) {
            assert(nc < HC);
            rep[nc] = p;
            for (unsigned h = 0; h < ht; h++) {
                uint8_t composed[CUBIES];
                for (unsigned i = 0; i < CUBIES; i++)
                    composed[i] = perm[hr[h]][perm[p][i]];
                unsigned n = cube_rank_permutation(composed);
                assert(pc[n] == 255);
                pc[n] = nc;
            }
            nc++;
        }
    assert(nc == HC);
    uint16_t cp[3][HC];
    for (unsigned f = 0; f < 3; f++)
        for (unsigned c = 0; c < HC; c++)
            cp[f][c] = pc[pt[f][rep[c]]];
    for (unsigned f = 0; f < 3; f++)
        for (unsigned p = 0; p < PERMUTATIONS; p++)
            assert(pc[pt[f][p]] == cp[f][pc[p]]);
    uint8_t *hd = bfs(HS, 729, &cp[0][0], &ot[0][0]);
    for (unsigned i = 0; i < HS; i++) {
        assert(hd[i] <= 11);
        setn(hp, i, hd[i]);
        hm[i / 4] |= (uint8_t) ((hd[i] % 3) << ((i % 4) * 2));
    }
    for (unsigned r = 0; r < HS; r++)
        for (unsigned f = 0; f < 3; f++) {
            unsigned c = r / 729, o = r % 729;
            for (unsigned k = 0; k < 3; k++) {
                c = cp[f][c];
                o = ot[f][o];
                unsigned n = c * 729 + o;
                int delta = (int) (hd[n] % 3) - (int) (hd[r] % 3);
                if (delta == -2)
                    delta = 1;
                else if (delta == 2)
                    delta = -1;
                assert((int) hd[r] + delta == hd[n]);
            }
        }
    if (verify_modulo3)
        for (unsigned c = 0; c < HC; c++)
            for (unsigned o = 0; o < 729; o++)
                assert(bounds_root_half(rep[c], o, NULL) == hd[c * 729 + o]);
    for (unsigned start = 0; start < HS; start += 2) {
        uint8_t low = hd[start];
        for (unsigned k = 1; k < 2 && start + k < HS; k++)
            if (hd[start + k] < low)
                low = hd[start + k];
        setn(hp2, start / 2, low);
    }
    for (unsigned start = 0; start < HS; start += 4) {
        uint8_t low = hd[start];
        for (unsigned k = 1; k < 4 && start + k < HS; k++)
            if (hd[start + k] < low)
                low = hd[start + k];
        setn(hp4, start / 4, low);
    }
    for (unsigned i = 0; i < HS; i++) {
        assert(nib(hp, i) == hd[i]);
        assert(((hm[i / 4] >> ((i % 4) * 2)) & 3) == hd[i] % 3);
        for (unsigned factor = 2; factor <= 4; factor *= 2) {
            unsigned start = (i / factor) * factor;
            uint8_t value = hd[start];
            for (unsigned j = start + 1; j < start + factor && j < HS; j++)
                if (hd[j] < value)
                    value = hd[j];
            assert(nib(factor == 2 ? hp2 : hp4, i / factor) == value);
        }
    }
    free(hd);
    for (unsigned r = 0; r < STATES; r++) {
        unsigned p = r / 729, o = r % 729;
        uint8_t a = nib(pd, p), b = nib(od, o),
                c = nib(hp, (uint32_t) pc[p] * 729 + o);
        assert(a <= exact[r] && b <= exact[r] && c <= exact[r] && c >= b);
        unsigned key = (uint32_t) pc[p] * 729 + o;
        assert(nib(hp2, key / 2) <= c && nib(hp4, key / 4) <= c);
    }
    for (unsigned ix = 0; ix < 2; ix++) {
        unsigned radius = ix == 0 ? 4 : 5;
        for (unsigned r = 0; r < STATES; r++)
            if (exact[r] <= radius)
                end_n[ix]++;
        assert(end_n[ix] == (ix == 0 ? 2232 : 12224));
        ends[ix] = malloc((size_t) end_n[ix] * 4);
        assert(ends[ix]);
        unsigned j = 0;
        for (unsigned r = 0; r < STATES; r++)
            if (exact[r] <= radius) {
                unsigned move = 0;
                if (r)
                    while (move < 9 &&
                           exact[tables_next(r, move)] + 1 != exact[r])
                        move++;
                assert(move < 9);
                ends[ix][j++] = (r << 7) | (move << 3) | exact[r];
            }
    }
    fprintf(stderr,
            "Prepared exact oracle; all-state projected bounds and half-turn "
            "coset transitions verified.\n");
}

void tables_free(void)
{
    free(exact);
    free(ends[0]);
    free(ends[1]);
    exact = NULL;
    ends[0] = ends[1] = NULL;
}
