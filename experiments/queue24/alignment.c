/* Native layout inspection; no target performance claims. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct { uint8_t p[7], o[7]; } original_state;
typedef struct { uint8_t cubie[7]; } byte_state;
typedef struct { uint8_t bytes[3]; } byte_rank;
typedef struct { uint16_t low; uint8_t high; } natural_rank;
typedef struct __attribute__((packed)) {
    uint16_t low;
    uint8_t high;
} packed_rank;
typedef struct { _Alignas(4) uint8_t bytes[3]; } aligned_rank;
typedef struct { _Alignas(4) uint8_t cubie[7]; } aligned_state;
typedef struct { _Alignas(4) uint8_t rank[4][3]; } four_byte_ranks;
typedef struct { uint32_t words[3]; } three_words;

#define SHOW(type) \
    printf("%-20s size=%2zu alignment=%zu array_stride=%2zu\n", \
           #type, sizeof(type), _Alignof(type), sizeof(type))

int main(void)
{
    SHOW(original_state);
    SHOW(byte_state);
    SHOW(byte_rank);
    SHOW(natural_rank);
    SHOW(packed_rank);
    SHOW(aligned_rank);
    SHOW(aligned_state);
    SHOW(four_byte_ranks);
    SHOW(three_words);
    printf("natural_rank.high offset=%zu\n", offsetof(natural_rank, high));
    four_byte_ranks group = {0};
    for (unsigned i = 0; i < 4; ++i)
        printf("rank[%u] offset=%zu\n", i,
               (size_t) (group.rank[i] - group.rank[0]));
    return 0;
}
