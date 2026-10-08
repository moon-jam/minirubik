#include <stdint.h>

#ifndef RV32I_REFERENCE
#include <stdio.h>
#include <string.h>
#endif

enum {
    CUBIES = 7,
    PERMUTATIONS = 5040,
    ORIENTATIONS = 729,
    STATES = PERMUTATIONS * ORIENTATIONS,
    MOVES = 9,
    HALF_CLASSES = 210,
    HALF_ENTRIES = HALF_CLASSES * ORIENTATIONS,
    MAX_DEPTH = 11
};

typedef struct {
    uint8_t p[CUBIES], o[CUBIES];
} state_t;

typedef struct {
    uint16_t p, o;
} coordinate_t;

typedef struct {
    uint8_t length, moves[MAX_DEPTH];
} solution_t;

/*@ predicate valid_state(state_t *state) =
      (\forall integer i; 0 <= i < CUBIES ==>
         state->p[i] < CUBIES && state->o[i] < 3) &&
      (\forall integer i, j; 0 <= i < j < CUBIES ==>
         state->p[i] != state->p[j]) &&
      (state->o[0] + state->o[1] + state->o[2] + state->o[3] +
       state->o[4] + state->o[5] + state->o[6]) % 3 == 0;
 */

static const char *const move_names[MOVES] = {"R",  "R2", "R'", "B", "B2",
                                              "B'", "D",  "D2", "D'"};
/* Eight-byte rows avoid multiplication by seven when selecting a face. */
static const uint8_t source[3][8] = {
    {1, 4, 2, 0, 3, 5, 6},
    {0, 1, 2, 4, 5, 6, 3},
    {0, 2, 5, 3, 1, 4, 6},
};
static const uint8_t twist[3][8] = {
    {1, 2, 0, 2, 1, 0, 0},
    {0, 0, 0, 1, 2, 1, 2},
    {0, 0, 0, 0, 0, 0, 0},
};

/* The three quarter-turns preserve the fixed front-upper-left corner. */
/*@ requires face < 3;
    assigns \nothing;
    ensures \forall integer i; 0 <= i < CUBIES ==>
              \result.p[i] == state.p[source[face][i]];
    ensures \forall integer i; 0 <= i < CUBIES ==>
              \result.o[i] == (state.o[source[face][i]] + twist[face][i]) % 3;
 */
static state_t quarter_turn(state_t state, uint8_t face)
{
    state_t result;
    /*@ loop invariant 0 <= i <= CUBIES;
        loop invariant \forall integer j; 0 <= j < i ==>
          result.p[j] == state.p[source[face][j]];
        loop invariant \forall integer j; 0 <= j < i ==>
          result.o[j] == (state.o[source[face][j]] + twist[face][j]) % 3;
        loop assigns i, result.p[0..6], result.o[0..6];
        loop variant CUBIES - i;
    */
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t from = source[face][i];
        result.p[i] = state.p[from];
        unsigned orientation = state.o[from] + twist[face][i];
        result.o[i] = (uint8_t) (orientation < 3 ? orientation : orientation - 3);
    }
    return result;
}

static state_t apply_move(state_t state, uint8_t move)
{
    uint8_t face = 0;
    if (move >= 6) {
        face = 2;
        move = (uint8_t) (move - 6);
    } else if (move >= 3) {
        face = 1;
        move = (uint8_t) (move - 3);
    }
    for (uint8_t i = 0; i <= move; ++i)
        state = quarter_turn(state, face);
    return state;
}

/*@ requires \valid_read(state);
    requires \forall integer i; 0 <= i < CUBIES ==>
      0 <= state->p[i] < CUBIES;
    requires \forall integer i, j; 0 <= i < j < CUBIES ==>
      state->p[i] != state->p[j];
    requires \forall integer i; 0 <= i < CUBIES ==>
      0 <= state->o[i] < 3;
    assigns \nothing;
    ensures \result.p < PERMUTATIONS && \result.o < ORIENTATIONS;
 */
static coordinate_t encode_state(const state_t *state)
{
    uint8_t digits[CUBIES - 1];
    uint32_t o = 0;
    /*@ loop invariant 0 <= i <= CUBIES - 1;
        loop invariant \forall integer j; 0 <= j < i ==> digits[j] <= 6 - j;
        loop assigns i, digits[0..5];
        loop variant CUBIES - 1 - i;
     */
    for (uint8_t i = 0; i < CUBIES - 1; ++i) {
        uint8_t smaller = 0;
        /*@ loop invariant i + 1 <= j <= CUBIES;
            loop invariant smaller <= j - i - 1;
            loop assigns j, smaller;
            loop variant CUBIES - j;
         */
        for (uint8_t j = (uint8_t) (i + 1U); j < CUBIES; ++j)
            smaller = (uint8_t) (smaller + (state->p[j] < state->p[i]));
        digits[i] = smaller;
    }
    uint32_t p = digits[0];
    p = (p << 2) + (p << 1) + digits[1]; /* Radix 6. */
    p = (p << 2) + p + digits[2];        /* Radix 5. */
    p = (p << 2) + digits[3];            /* Radix 4. */
    p = (p << 1) + p + digits[4];        /* Radix 3. */
    p = (p << 1) + digits[5];            /* Radix 2; final radix-1 digit is zero. */
    /*@ loop invariant 0 <= i <= 6;
        loop invariant (i == 0 ==> o == 0) && (i == 1 ==> o < 3) &&
          (i == 2 ==> o < 9) && (i == 3 ==> o < 27) &&
          (i == 4 ==> o < 81) && (i == 5 ==> o < 243) &&
          (i == 6 ==> o < 729);
        loop assigns i, o;
        loop variant 6 - i;
     */
    for (uint8_t i = 0; i < 6; ++i)
        o = (o << 1) + o + state->o[i];
    return (coordinate_t) {(uint16_t) p, (uint16_t) o};
}

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
static int valid(const state_t *state)
{
    uint8_t sum = 0;
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
    for (uint8_t i = 0; i < CUBIES; ++i) {
        if (state->p[i] >= CUBIES || state->o[i] >= 3)
            return 0;
        /*@ loop invariant 0 <= j <= i;
            loop invariant \forall integer k; 0 <= k < j ==>
              state->p[k] != state->p[i];
            loop assigns j;
            loop variant i - j;
        */
        for (uint8_t j = 0; j < i; ++j)
            if (state->p[j] == state->p[i])
                return 0;
        sum = (uint8_t) (sum + state->o[i]);
    }
    /* Bit positions 0, 3, 6, 9, and 12 are the legal sums (sum <= 14). */
    return (int) ((UINT32_C(0x1249) >> sum) & 1U);
}

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
static int parse_state(const char *input, state_t *state)
{
    /*@ loop invariant 0 <= i <= CUBIES;
        loop invariant i <= strlen(input);
        loop invariant \initialized(&state->p[0..i-1]);
        loop invariant \forall integer j; 0 <= j < i ==>
          state->p[j] == input[j] - '1';
        loop assigns i, state->p[0..6];
        loop variant CUBIES - i;
     */
    for (int i = 0; i < CUBIES; ++i) {
        if (input[i] < '1' || input[i] > '7')
            return 0;
        state->p[i] = (uint8_t) (input[i] - '1');
    }
    /*@ loop invariant 0 <= i <= CUBIES;
        loop invariant i + CUBIES <= strlen(input);
        loop invariant \initialized(&state->p[0..6]);
        loop invariant \initialized(&state->o[0..i-1]);
        loop invariant \forall integer j; 0 <= j < i ==>
          state->o[j] == input[j + CUBIES] - '1';
        loop assigns i, state->o[0..6];
        loop variant CUBIES - i;
     */
    for (int i = 0; i < CUBIES; ++i) {
        if (input[i + CUBIES] < '1' || input[i + CUBIES] > '3')
            return 0;
        state->o[i] = (uint8_t) (input[i + CUBIES] - '1');
    }
    return input[14] == '\0' && valid(state);
}

#include "solver-tables.inc"

/* Power-of-two pointer strides avoid runtime multiplication by row widths. */
static const uint16_t *const permutation_rows[3] = {
    permutation_transitions[0], permutation_transitions[1], permutation_transitions[2]
};
static const uint16_t *const orientation_rows[3] = {
    orientation_transitions[0], orientation_transitions[1], orientation_transitions[2]
};

typedef struct {
    uint16_t p, o, next_p, next_o;
    uint8_t distance, previous_face, face, turn;
    uint8_t next_distance, next_remainder, remainder;
} search_frame_t;

static search_frame_t frames[MAX_DEPTH + 1];

static uint64_t attempts, expanded, passes, face_groups, root_trials, root_steps;

static void begin_frame(search_frame_t *frame, uint16_t p, uint16_t o,
                        uint8_t distance, uint8_t previous_face,
                        uint8_t remainder)
{
    frame->p = p;
    frame->o = o;
    frame->distance = distance;
    frame->previous_face = previous_face;
    frame->face = frame->turn = 0;
    frame->remainder = remainder;
}

static uint8_t permutation_bound(uint16_t p)
{
    return (uint8_t) ((permutation_distance[p >> 1] >> ((p & 1U) << 2)) & 15U);
}

static uint8_t half_remainder(uint16_t p, uint16_t o)
{
    uint32_t index = subgroup_offset[p] + o;
    return (uint8_t) ((subgroup_remainder[index >> 2] >>
                      ((index & 3U) << 1)) & 3U);
}

/* Recover the root distance without changing the original input. */
static int root_half_distance(coordinate_t state, uint8_t remainder)
{
    unsigned distance = 0;
    while (subgroup_offset[state.p] != 0 || state.o != 0) {
        uint8_t wanted = remainder == 0 ? 2 : (uint8_t) (remainder - 1);
        int found = 0;
        for (unsigned face = 0; face < 3 && !found; ++face) {
            coordinate_t next = state;
            for (unsigned turn = 0; turn < 3; ++turn) {
                next.p = permutation_rows[face][next.p];
                next.o = orientation_rows[face][next.o];
                ++root_trials;
                if (half_remainder(next.p, next.o) == wanted) {
                    state = next;
                    remainder = wanted;
                    found = 1;
                    break;
                }
            }
        }
        if (!found || ++distance > MAX_DEPTH)
            return -1;
        ++root_steps;
    }
    return (int) distance;
}

/* Each frame resumes its face/turn loop after a child returns. */
static int search(coordinate_t root, solution_t *solution)
{
    attempts = expanded = passes = face_groups = root_trials = root_steps = 0;
    uint8_t root_remainder = half_remainder(root.p, root.o);
    int distance = root_half_distance(root, root_remainder);
    if (distance < 0)
        return -1;
    if (root.p == 0 && root.o == 0) {
        solution->length = 0;
        return 0;
    }
    unsigned first_bound = permutation_bound(root.p);
    if ((unsigned) distance > first_bound)
        first_bound = (unsigned) distance;
    for (unsigned bound = first_bound; bound <= MAX_DEPTH; ++bound) {
        ++passes;
        unsigned depth = 0;
        begin_frame(&frames[0], root.p, root.o, (uint8_t) distance, UINT8_MAX,
                    root_remainder);
        ++expanded;
        for (;;) {
            search_frame_t *frame = &frames[depth];
            if (frame->face == frame->previous_face) {
                ++frame->face;
                frame->turn = 0;
            }
            if (frame->face == 3) {
                if (depth == 0)
                    break;
                --depth;
                continue;
            }
            unsigned face = frame->face;
            if (frame->turn == 0) {
                ++face_groups;
                frame->next_p = frame->p;
                frame->next_o = frame->o;
                frame->next_distance = frame->distance;
                frame->next_remainder = frame->remainder;
            }
            frame->next_p = permutation_rows[face][frame->next_p];
            frame->next_o = orientation_rows[face][frame->next_o];
            uint8_t remainder = half_remainder(frame->next_p, frame->next_o);
            int delta = (int) remainder - frame->next_remainder;
            if (delta == -2)
                delta = 1;
            else if (delta == 2)
                delta = -1;
            frame->next_distance = (uint8_t) (frame->next_distance + delta);
            frame->next_remainder = remainder;
            ++attempts;
            uint8_t move = (uint8_t) ((face << 1) + face + frame->turn);
            if (++frame->turn == 3) {
                ++frame->face;
                frame->turn = 0;
            }
            unsigned h = permutation_bound(frame->next_p);
            if (frame->next_distance > h)
                h = frame->next_distance;
            if (depth + 1 + h > bound)
                continue;
            solution->moves[depth] = move;
            if (frame->next_p == 0 && frame->next_o == 0) {
                solution->length = (uint8_t) (depth + 1);
                return (int) solution->length;
            }
            if (depth + 1 == bound)
                continue;
            begin_frame(&frames[depth + 1], frame->next_p, frame->next_o,
                        frame->next_distance, (uint8_t) face, remainder);
            ++depth;
            ++expanded;
        }
    }
    return -1;
}

static int is_solved(const state_t *state)
{
    for (unsigned i = 0; i < CUBIES; ++i)
        if (state->p[i] != i || state->o[i] != 0)
            return 0;
    return 1;
}

/* Returns -2 for invalid input, -1 for a failed search/replay, or its length.
 * Fixed frames make this a single-query, non-reentrant interface.
 */
int solve_cube(const char *input, solution_t *solution)
{
    state_t state;
    if (!input || !solution || !parse_state(input, &state))
        return -2;
    int length = search(encode_state(&state), solution);
    if (length < 0)
        return -1;
    for (unsigned i = 0; i < solution->length; ++i)
        state = apply_move(state, solution->moves[i]);
    return is_solved(&state) ? length : -1;
}

#ifndef RV32I_REFERENCE
/* Buffered output errors may surface only at the flush. */
static int output_failed(void)
{
    return fflush(stdout) != 0 || ferror(stdout);
}


int main(int argc, char **argv)
{
    solution_t solution;
    int length = argc == 2 ? solve_cube(argv[1], &solution) : -2;
    if (length == -2) {
        fprintf(stderr, "usage: %s PPPPPPPOOOOOOO\n",
                argc > 0 && argv[0] ? argv[0] : "solver");
        return 2;
    }
    if (length < 0) {
        fputs("search or solution replay failed\n", stderr);
        return 1;
    }
    for (unsigned i = 0; i < solution.length; ++i)
        printf("%s%s", i ? " " : "", move_names[solution.moves[i]]);
    putchar('\n');
    return output_failed();
}
#endif
