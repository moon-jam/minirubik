#include "tables.h"
#include "bounds.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef NDEBUG
#error Search experiments require assertions; do not define NDEBUG.
#endif
/* Construction runs on the host. export.c selects the retained arrays;
 * the oracle and comparison-only tables below are not target data. */
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
    /* Even entries occupy bits 0..3, odd entries bits 4..7. Clear only
     * the selected nibble, preserving its neighbor in the same byte. */
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
    /* Breadth-first search (BFS) on ranks p*width + o, starting at rank 0.
     * a advances p; optional b advances o. With width=1 and b=NULL this
     * searches a single coordinate. Unvisited distances use the value 255.
     * Each face supplies quarter, half, and inverse turns by one, two, or
     * three transition lookups. All cost ONE move in the half-turn metric,
     * so every newly discovered neighbor receives d[r]+1, not d[r]+k+1.
     * Each move's inverse is available, making these distances to the
     * starting target as well as distances from it. */
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
    /* Full-state BFS is a host-only correctness oracle. Its histogram
     * checks the diameter and identifies the 2644 hardest test inputs. */
    exact = bfs(STATES, 729, &pt[0][0], &ot[0][0]);
    unsigned hist[12] = {0};
    for (unsigned r = 0; r < STATES; r++) {
        assert(exact[r] <= 11);
        hist[exact[r]]++;
    }
    assert(hist[11] == 2644 && hist[0] == 1);
    /* Project away one coordinate at a time. Any complete solution also
     * solves each projection, so these exact projected distances are lower
     * bounds for the full problem. Pack two distances into each byte. */
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
    /* Close the identity under R2, B2, D2, appending only new permutation
     * ranks. These half turns preserve zero orientation. Processing every
     * appended entry enumerates the subgroup H; closure yields 24 states. */
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
    /* Partition permutations into right cosets Hs. In move-sequence order,
     * h followed by s has permutation h[s[i]], the composition below.
     * H has zero orientation, so these classes can be paired with all 729
     * orientation ranks without storing a full-state class map. The first
     * class contains identity and therefore represents the target H. */
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
    /* Build class transitions from representatives, then check every
     * member: applying the same move must reach the same successor class.
     * This is what makes the reduced graph well-defined. */
    uint16_t cp[3][HC];
    for (unsigned f = 0; f < 3; f++)
        for (unsigned c = 0; c < HC; c++)
            cp[f][c] = pc[pt[f][rep[c]]];
    for (unsigned f = 0; f < 3; f++)
        for (unsigned p = 0; p < PERMUTATIONS; p++)
            assert(pc[pt[f][p]] == cp[f][pc[p]]);
    /* BFS over 210*729 class/orientation pairs gives exact distance to H.
     * Since solved belongs to H, this is a lower bound for solving fully.
     * Keep both four-bit distances (hp) and two-bit residues (hm) for the
     * storage experiments; the selected solver exports only hm. */
    uint8_t *hd = bfs(HS, 729, &cp[0][0], &ot[0][0]);
    for (unsigned i = 0; i < HS; i++) {
        assert(hd[i] <= 11);
        setn(hp, i, hd[i]);
        hm[i / 4] |= (uint8_t) ((hd[i] % 3) << ((i % 4) * 2));
    }
    /* Neighbor distances differ by at most one. D-1, D, D+1 have distinct
     * residues, so a known parent distance and the child's residue recover
     * the exact child distance. Check this for all nine outgoing moves. */
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
    /* The root has no known parent distance. Verify the separate procedure
     * that follows decreasing residues until actual membership in H;
     * residue zero alone is insufficient (distance three also has it). */
    if (verify_modulo3)
        for (unsigned c = 0; c < HC; c++)
            for (unsigned o = 0; o < 729; o++)
                assert(bounds_root_half(rep[c], o, NULL) == hd[c * 729 + o]);
    /* Lossy alternatives merge consecutive entries in blocks of two/four.
     * Store each block's minimum: it cannot exceed any member's original
     * lower bound, so admissibility is preserved but pruning may weaken. */
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
    /* Check the bit layouts and merged minima against unpacked distances
     * before releasing the temporary subgroup-distance array. */
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
    /* Exhaustively compare projected bounds with the full-state oracle.
     * H is inside the zero-orientation target, so its distance also
     * dominates the orientation-only distance. */
    for (unsigned r = 0; r < STATES; r++) {
        unsigned p = r / 729, o = r % 729;
        uint8_t a = nib(pd, p), b = nib(od, o),
                c = nib(hp, (uint32_t) pc[p] * 729 + o);
        assert(a <= exact[r] && b <= exact[r] && c <= exact[r] && c >= b);
        unsigned key = (uint32_t) pc[p] * 729 + o;
        assert(nib(hp2, key / 2) <= c && nib(hp4, key / 4) <= c);
    }
    /* Experiment-only perimeter tables retain states within radius 4/5.
     * Choose a move that decreases exact distance to replay a solved suffix.
     * Records are sorted by rank for binary search and packed as
     * [rank in bits 7..31 | move in bits 3..6 | distance in bits 0..2].
     * The move field is unused at distance zero. */
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
