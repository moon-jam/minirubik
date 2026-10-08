#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    CUBIES = 7,
    PERMUTATIONS = 5040,
    ORIENTATIONS = 729,
    STATES = PERMUTATIONS * ORIENTATIONS,
    MOVES = 9
};

/* Default storage is seven packed bytes.
 * STATE_LAYOUT=0 restores the original arrays for comparison and proof.
 */
#ifndef STATE_LAYOUT
#define STATE_LAYOUT 1
#endif
#if STATE_LAYOUT == 0
typedef struct { uint8_t p[CUBIES], o[CUBIES]; } state_t;
#define state_p(s, i) ((s)->p[i])
#define state_o(s, i) ((s)->o[i])
#define state_set_p(s, i, value) ((s)->p[i] = (value))
#define state_set_o(s, i, value) ((s)->o[i] = (value))
#define state_set(s, i, p_value, o_value) do { \
    state_set_p(s, i, p_value); \
    state_set_o(s, i, o_value); \
} while (0)
#elif STATE_LAYOUT == 1
typedef struct { uint8_t cubie[CUBIES]; } state_t;
typedef char state_must_be_seven_bytes[(sizeof(state_t) == 7) ? 1 : -1];
/* Low four bits: cubie label. High four bits: orientation. */
static uint8_t state_p(const state_t *s, unsigned i)
{ return (uint8_t) (s->cubie[i] & 15U); }
static uint8_t state_o(const state_t *s, unsigned i)
{ return (uint8_t) (s->cubie[i] >> 4); }
static void state_set_p(state_t *s, unsigned i, uint8_t p)
{ s->cubie[i] = (uint8_t) ((s->cubie[i] & 0xf0U) | p); }
static void state_set_o(state_t *s, unsigned i, uint8_t o)
{ s->cubie[i] = (uint8_t) ((s->cubie[i] & 15U) | (o << 4)); }
static void state_set(state_t *s, unsigned i, uint8_t p, uint8_t o)
{ s->cubie[i] = (uint8_t) (p | (o << 4)); }
#else
#error Unsupported STATE_LAYOUT
#endif


#if STATE_LAYOUT == 0
/*@ predicate valid_state(state_t *state) =
      (\forall integer i; 0 <= i < CUBIES ==>
         state->p[i] < CUBIES && state->o[i] < 3) &&
      (\forall integer i, j; 0 <= i < j < CUBIES ==>
         state->p[i] != state->p[j]) &&
      (state->o[0] + state->o[1] + state->o[2] + state->o[3] +
       state->o[4] + state->o[5] + state->o[6]) % 3 == 0;
 */
#endif

static const char *const move_names[MOVES] = {"R",  "R2", "R'", "B", "B2",
                                              "B'", "D",  "D2", "D'"};
static const uint8_t inverse_move[MOVES] = {2, 1, 0, 5, 4, 3, 8, 7, 6};
/* Each destination takes a cubie from source[face][destination]. */
static const uint8_t source[3][CUBIES] = {
    {1, 4, 2, 0, 3, 5, 6},
    {0, 1, 2, 4, 5, 6, 3},
    {0, 2, 5, 3, 1, 4, 6},
};
static const uint8_t twist[3][CUBIES] = {
    {1, 2, 0, 2, 1, 0, 0},
    {0, 0, 0, 1, 2, 1, 2},
    {0, 0, 0, 0, 0, 0, 0},
};

/* The three quarter-turns preserve the fixed front-upper-left corner. */
#if STATE_LAYOUT == 0
/*@ requires face < 3;
    assigns \nothing;
    ensures \forall integer i; 0 <= i < CUBIES ==>
              \result.p[i] == state.p[source[face][i]];
    ensures \forall integer i; 0 <= i < CUBIES ==>
              \result.o[i] == (state.o[source[face][i]] + twist[face][i]) % 3;
 */
#endif
static state_t quarter_turn(state_t state, uint8_t face)
{
    state_t result;
    #if STATE_LAYOUT == 0
/*@ loop invariant 0 <= i <= CUBIES;
        loop invariant \forall integer j; 0 <= j < i ==>
          result.p[j] == state.p[source[face][j]];
        loop invariant \forall integer j; 0 <= j < i ==>
          result.o[j] == (state.o[source[face][j]] + twist[face][j]) % 3;
        loop assigns i, result.p[0..6], result.o[0..6];
        loop variant CUBIES - i;
    */
#endif
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t from = source[face][i];
        state_set(&result, i, state_p(&state, from),
                  (uint8_t) ((state_o(&state, from) + twist[face][i]) % 3U));
    }
    return result;
}

static state_t apply_move(state_t state, uint8_t move)
{
    uint8_t turns = (uint8_t) (move % 3U + 1U);
    for (uint8_t i = 0; i < turns; ++i)
        state = quarter_turn(state, (uint8_t) (move / 3U));
    return state;
}

#if STATE_LAYOUT == 0
/*@ requires \valid_read(state);
    requires \forall integer i; 0 <= i < CUBIES ==>
      0 <= state->p[i] < CUBIES;
    requires \forall integer i, j; 0 <= i < j < CUBIES ==>
      state->p[i] != state->p[j];
    requires \forall integer i; 0 <= i < CUBIES ==>
      0 <= state->o[i] < 3;
    assigns \nothing;
    ensures \result < STATES;
 */
#endif
static uint32_t rank_state(const state_t *state)
{
    uint32_t p = 0, o = 0;
    #if STATE_LAYOUT == 0
/*@ loop invariant 0 <= i <= CUBIES;
        loop invariant (i == 0 ==> p == 0) && (i == 1 ==> p <= 6) &&
          (i == 2 ==> p <= 41) && (i == 3 ==> p <= 209) &&
          (i == 4 ==> p <= 839) && (i == 5 ==> p <= 2519) &&
          (i >= 6 ==> p <= 5039);
        loop assigns i, p;
        loop variant CUBIES - i;
     */
#endif
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t smaller = 0;
        #if STATE_LAYOUT == 0
/*@ loop invariant i + 1 <= j <= CUBIES;
            loop invariant smaller <= j - i - 1;
            loop assigns j, smaller;
            loop variant CUBIES - j;
         */
#endif
        for (uint8_t j = (uint8_t) (i + 1U); j < CUBIES; ++j)
            if (state_p(state, j) < state_p(state, i))
                ++smaller;
        p = p * (CUBIES - i) + smaller;
    }
    #if STATE_LAYOUT == 0
/*@ loop invariant 0 <= i <= 6;
        loop invariant (i == 0 ==> o == 0) && (i == 1 ==> o < 3) &&
          (i == 2 ==> o < 9) && (i == 3 ==> o < 27) &&
          (i == 4 ==> o < 81) && (i == 5 ==> o < 243) &&
          (i == 6 ==> o < 729);
        loop assigns i, o;
        loop variant 6 - i;
     */
#endif
    for (uint8_t i = 0; i < 6; ++i)
        o = o * 3U + state_o(state, i);
    return p * ORIENTATIONS + o;
}

#if STATE_LAYOUT == 0
/*@ requires \valid(state); requires rank < STATES; assigns *state; */
#endif
static void unrank_state(uint32_t rank, state_t *state)
{
#if STATE_LAYOUT == 1
    /* Field setters preserve the other nibble, so initialize both first. */
    memset(state, 0, sizeof *state);
#endif
    uint8_t available[CUBIES] = {0, 1, 2, 3, 4, 5, 6};
    uint32_t p = rank / ORIENTATIONS, o = rank % ORIENTATIONS, f = 720;
    uint8_t sum = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t q = (uint8_t) (p / f);
        p %= f;
        state_set_p(state, i, available[q]);
        for (uint8_t j = q; j + 1U < CUBIES - i; ++j)
            available[j] = available[j + 1U];
        if (i < 5)
            f /= 6U - i;
    }
    for (uint8_t i = 6; i-- > 0;) {
        state_set_o(state, i, (uint8_t) (o % 3U));
        sum = (uint8_t) (sum + state_o(state, i));
        o /= 3U;
    }
    state_set_o(state, 6, (uint8_t) ((3U - sum % 3U) % 3U));
}

#if STATE_LAYOUT == 0
/*@ requires \valid_read(state);
    requires \initialized(&state->p[0..6]) && \initialized(&state->o[0..6]);
    assigns \nothing;
    ensures \result != 0 ==> \forall integer i; 0 <= i < CUBIES ==>
      state->p[i] < CUBIES && state->o[i] < 3;
    ensures \result != 0 ==> \forall integer i, j; 0 <= i < j < CUBIES ==>
      state->p[i] != state->p[j];
    ensures \result != 0 ==>
      (state->o[0] + state->o[1] + state->o[2] + state->o[3] +
       state->o[4] + state->o[5] + state->o[6]) % 3 == 0;
    ensures complete: valid_state(state) ==> \result != 0;
 */
#endif
static int valid(const state_t *state)
{
    uint8_t sum = 0;
    #if STATE_LAYOUT == 0
/*@ loop invariant 0 <= i <= CUBIES;
        loop invariant sum <= 2 * i;
        loop invariant sum == (i > 0 ? state->o[0] : 0) +
          (i > 1 ? state->o[1] : 0) + (i > 2 ? state->o[2] : 0) +
          (i > 3 ? state->o[3] : 0) + (i > 4 ? state->o[4] : 0) +
          (i > 5 ? state->o[5] : 0) + (i > 6 ? state->o[6] : 0);
        loop invariant \forall integer j; 0 <= j < i ==>
          state->p[j] < CUBIES && state->o[j] < 3;
        loop invariant \forall integer j, k; 0 <= j < k < i ==>
          state->p[j] != state->p[k];
        loop assigns i, sum;
        loop variant CUBIES - i;
    */
#endif
    for (uint8_t i = 0; i < CUBIES; ++i) {
        if (state_p(state, i) >= CUBIES || state_o(state, i) >= 3)
            return 0;
        #if STATE_LAYOUT == 0
/*@ loop invariant 0 <= j <= i;
            loop invariant \forall integer k; 0 <= k < j ==>
              state->p[k] != state->p[i];
            loop assigns j;
            loop variant i - j;
        */
#endif
        for (uint8_t j = 0; j < i; ++j)
            if (state_p(state, j) == state_p(state, i))
                return 0;
        sum = (uint8_t) (sum + state_o(state, i));
    }
    return sum % 3U == 0;
}

/* QUEUE_LAYOUT: 0 = uint32_t, 1 = three bytes, 2 = packed uint16_t/uint8_t. */
#ifndef QUEUE_LAYOUT
#define QUEUE_LAYOUT 1
#endif

#if QUEUE_LAYOUT == 1
typedef struct { uint8_t bytes[3]; } queue_entry_t;
typedef char queue_entry_is_three_bytes[sizeof(queue_entry_t) == 3 ? 1 : -1];
static inline uint32_t queue_read(const queue_entry_t *queue, uint32_t index)
{
    const uint8_t *b = queue[index].bytes;
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16);
}
static inline void queue_write(queue_entry_t *queue, uint32_t index, uint32_t rank)
{
    queue[index].bytes[0] = (uint8_t)rank;
    queue[index].bytes[1] = (uint8_t)(rank >> 8);
    queue[index].bytes[2] = (uint8_t)(rank >> 16);
}
#elif QUEUE_LAYOUT == 2
typedef struct __attribute__((packed)) {
    uint16_t low;
    uint8_t high;
} queue_entry_t;
typedef char queue_entry_is_three_bytes[sizeof(queue_entry_t) == 3 ? 1 : -1];
static inline uint32_t queue_read(const queue_entry_t *queue, uint32_t index)
{
    return (uint32_t)queue[index].low | ((uint32_t)queue[index].high << 16);
}
static inline void queue_write(queue_entry_t *queue, uint32_t index, uint32_t rank)
{
    queue[index].low = (uint16_t)rank;
    queue[index].high = (uint8_t)(rank >> 16);
}
#elif QUEUE_LAYOUT == 0
typedef uint32_t queue_entry_t;
static inline uint32_t queue_read(const queue_entry_t *queue, uint32_t index)
{
    return queue[index];
}
static inline void queue_write(queue_entry_t *queue, uint32_t index, uint32_t rank)
{
    queue[index] = rank;
}
#else
#error Unsupported QUEUE_LAYOUT
#endif

static uint8_t *build_table(uint8_t *diameter)
{
    uint8_t *toward_solved = malloc(STATES);
    queue_entry_t *queue = malloc((size_t) STATES * sizeof *queue);
    uint16_t permutation[3][PERMUTATIONS], orientation[3][ORIENTATIONS];
    uint32_t head = 0, tail = 1, level_end = 1;
    state_t state;
    if (!toward_solved || !queue) {
        free(toward_solved);
        free(queue);
        return NULL;
    }
    for (uint16_t rank = 0; rank < PERMUTATIONS; ++rank) {
        unrank_state((uint32_t) rank * ORIENTATIONS, &state);
        for (uint8_t face = 0; face < 3; ++face) {
            state_t next = quarter_turn(state, face);
            permutation[face][rank] =
                (uint16_t) (rank_state(&next) / ORIENTATIONS);
        }
    }
    for (uint16_t rank = 0; rank < ORIENTATIONS; ++rank) {
        unrank_state(rank, &state);
        for (uint8_t face = 0; face < 3; ++face) {
            state_t next = quarter_turn(state, face);
            orientation[face][rank] =
                (uint16_t) (rank_state(&next) % ORIENTATIONS);
        }
    }
    memset(toward_solved, UINT8_MAX, STATES);
    queue_write(queue, 0, 0);
    toward_solved[0] = 0;
    *diameter = 0;
    while (head < tail) {
        if (head == level_end) {
            level_end = tail;
            ++*diameter;
        }
        uint32_t here = queue_read(queue, head++);
        uint16_t p = (uint16_t) (here / ORIENTATIONS);
        uint16_t o = (uint16_t) (here % ORIENTATIONS);
        for (uint8_t face = 0; face < 3; ++face) {
            uint16_t next_p = p, next_o = o;
            for (uint8_t turn = 0; turn < 3; ++turn) {
                next_p = permutation[face][next_p];
                next_o = orientation[face][next_o];
                uint32_t there = (uint32_t) next_p * ORIENTATIONS + next_o;
                if (toward_solved[there] == UINT8_MAX) {
                    uint8_t move = (uint8_t) (face * 3U + turn);
                    toward_solved[there] = inverse_move[move];
                    queue_write(queue, tail++, there);
                }
            }
        }
    }
    free(queue);
    if (tail != STATES) {
        free(toward_solved);
        return NULL;
    }
    return toward_solved;
}

#if STATE_LAYOUT == 0
/*@ requires valid_read_string(input);
    requires \valid(state);
    assigns state->p[0..6], state->o[0..6];
    ensures \result != 0 ==> input[14] == '\0';
    ensures \result != 0 ==> \forall integer i; 0 <= i < CUBIES ==>
      state->p[i] < CUBIES && state->o[i] < 3;
    ensures \result != 0 ==> \forall integer i, j; 0 <= i < j < CUBIES ==>
      state->p[i] != state->p[j];
    ensures \result != 0 ==>
      (state->o[0] + state->o[1] + state->o[2] + state->o[3] +
       state->o[4] + state->o[5] + state->o[6]) % 3 == 0;
    ensures \result != 0 ==> \forall integer i; 0 <= i < CUBIES ==>
      state->p[i] == input[i] - '1';
    ensures \result != 0 ==> \forall integer i; 0 <= i < CUBIES ==>
      state->o[i] == input[i + CUBIES] - '1';
 */
#endif
static int parse_state(const char *input, state_t *state)
{
#if STATE_LAYOUT == 1
    /* Field setters preserve the other nibble, so initialize both first. */
    memset(state, 0, sizeof *state);
#endif
    #if STATE_LAYOUT == 0
/*@ loop invariant 0 <= i <= 14;
        loop invariant i <= strlen(input);
        loop invariant i <= 7 ==> \initialized(&state->p[0..i-1]);
        loop invariant i >= 7 ==> \initialized(&state->p[0..6]);
        loop invariant i >= 7 ==> \initialized(&state->o[0..i-8]);
        loop invariant \forall integer j; 0 <= j < i && j < CUBIES ==>
          state->p[j] == input[j] - '1';
        loop invariant \forall integer j; 0 <= j < i - CUBIES ==>
          state->o[j] == input[j + CUBIES] - '1';
        loop assigns i, state->p[0..6], state->o[0..6];
        loop variant 14 - i;
     */
#endif
    for (int i = 0; i < 14; ++i) {
        int limit = i < 7 ? 7 : 3;
        if (input[i] < '1' || input[i] > '0' + limit)
            return 0;
#if STATE_LAYOUT == 0
        (i < 7 ? state->p : state->o)[i % 7] = (uint8_t) (input[i] - '1');
#else
        if (i < CUBIES)
            state_set_p(state, (unsigned) i, (uint8_t) (input[i] - '1'));
        else
            state_set_o(state, (unsigned) (i - CUBIES), (uint8_t) (input[i] - '1'));
#endif
    }
    return input[14] == '\0' && valid(state);
}

/* stdout is fully buffered off a terminal, so a write error surfaces at the
 * flush, not at the printf that queued the bytes. Every exit path that has
 * produced output goes through here.
 */
static int output_failed(void)
{
    return fflush(stdout) != 0 || ferror(stdout);
}

static int self_test(void)
{
#if STATE_LAYOUT == 0
    const state_t solved = {{0, 1, 2, 3, 4, 5, 6}, {0}};
#else
    const state_t solved = {{0, 1, 2, 3, 4, 5, 6}};
#endif
    state_t state;
    for (uint8_t move = 0; move < MOVES; ++move) {
        state = solved;
        state = apply_move(state, move);
        state = apply_move(state, inverse_move[move]);
        if (memcmp(&solved, &state, sizeof solved))
            return 0;
    }
    for (uint32_t rank = 0; rank < STATES; ++rank) {
        unrank_state(rank, &state);
        if (!valid(&state) || rank_state(&state) != rank)
            return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    state_t state;
    uint8_t diameter;
    if (argc == 2 && !strcmp(argv[1], "--self-test")) {
        if (!self_test()) {
            fputs("self-test failed\n", stderr);
            return 1;
        }
        uint8_t *table = build_table(&diameter);
        if (!table) {
            fputs("could not build complete state table\n", stderr);
            return 1;
        }
        free(table);
        if (diameter != 11) {
            fputs("BFS check failed\n", stderr);
            return 1;
        }
        puts("3674160 states; diameter 11");
        return output_failed();
    }
    if (argc != 2 || !parse_state(argv[1], &state)) {
        /* C99 5.1.2.2.1 lets argv[0] be null when argc is 0. */
        fprintf(stderr, "usage: %s PPPPPPPOOOOOOO\n",
                argc > 0 && argv[0] ? argv[0] : "solver");
        return 2;
    }
    uint8_t *table = build_table(&diameter);
    if (!table) {
        fputs("could not build complete state table\n", stderr);
        return 1;
    }
    const char *separator = "";
    for (uint32_t rank = rank_state(&state); rank; rank = rank_state(&state)) {
        uint8_t move = table[rank];
        printf("%s%s", separator, move_names[move]);
        separator = " ";
        state = apply_move(state, move);
    }
    putchar('\n');
    free(table);
    return output_failed();
}
