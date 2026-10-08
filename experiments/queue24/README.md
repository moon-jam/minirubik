# Queue and cubie storage experiment

The reference solver and solution vectors are frozen in `../baseline/`; changes outside `experiments/` do not affect them. The experimental version in this directory compares 24-bit queue ranks and four-bit permutation/orientation fields, enabled by default with `STATE_LAYOUT=1` and `QUEUE_LAYOUT=1`. Setting both layouts to zero uses the original storage. Cube move semantics, ranking, BFS order and output are preserved. Original array specifications remain conditionally enabled for the array representation; formal verification of the compressed variant has not been performed.

`QUEUE_LAYOUT` selects:

| Value | Storage | Bytes per rank |
|---|---|---:|
| 0 | `uint32_t` | 4 |
| 1 | `uint8_t bytes[3]` | 3 |
| 2 | Packed `uint16_t low` + `uint8_t high` | 3 |

Layout 2 uses a GCC/Clang packing attribute. A natural, unpacked struct with those fields may occupy 4 bytes; the benchmark prints its actual size. Its packed 16-bit member may be unaligned, so it is not assumed to produce cheap or aligned target loads.

`STATE_LAYOUT=0` keeps the original two seven-byte arrays. `STATE_LAYOUT=1` uses seven bytes: each byte stores `p` in its low four bits and `o` in its high four bits. Reads use masks/shifts. Component setters preserve the other field, so parsing and unranking initialize the storage before using them. The BFS queue contains ranks, not `state_t` objects; halving `sizeof(state_t)` does not halve queue memory.

```sh
python3 experiments/queue24/run.py
```

Check all variants without repeating the native timings:

```sh
python3 experiments/queue24/run.py --verify-only
```

Build baseline and default compressed binaries separately:

```sh
cc -O3 -std=c99 -Wall -Wextra -Wpedantic -DSTATE_LAYOUT=0 -DQUEUE_LAYOUT=0 experiments/queue24/solver.c -o experiments/queue24/build/solver-baseline
cc -O3 -std=c99 -Wall -Wextra -Wpedantic experiments/queue24/solver.c -o experiments/queue24/build/solver-compressed
```

The runner verifies the uncompressed table against the saved upstream SHA-256, compares all 3,674,160 move-table entries for each configuration, verifies rank/unrank over every state, move inverses, every supplied solution vector and invalid inputs, then measures five fresh processes per configuration in rotating order. Timing covers `build_table()` using a monotonic clock; host peak resident memory comes from macOS `/usr/bin/time -l`. On an execution sandbox that blocks its kernel-statistics query, the runner needs permission to execute outside that sandbox.

Native results on 2026-10-06, Apple Clang 21, macOS arm64 (five-run medians):

| Configuration | State bytes | Queue entry bytes | Queue bytes | Build ms | Peak host bytes |
|---|---:|---:|---:|---:|---:|
| Original layouts | 14 | 4 | 14,696,640 | 91.150 | 19,841,024 |
| Three-byte queue | 14 | 3 | 11,022,480 | 95.844 | 16,171,008 |
| Packed uint16 + uint8 queue | 14 | 3 | 11,022,480 | 92.814 | 16,154,624 |
| Four-bit p/o fields | 7 | 4 | 14,696,640 | 90.650 | 19,857,408 |
| Four-bit p/o + three-byte queue | 7 | 3 | 11,022,480 | 92.922 | 16,171,008 |

The unpacked uint16/uint8 struct was four bytes on this installation. All configurations produced the same complete table. The queue saves exactly 3,674,160 bytes; the timing samples do not establish a speed improvement. Full observations and ranges are saved by the runner in `build/results.json`.

These are native measurements, not RV32I/Ripes instruction counts. They do not establish target speed or compliance with the 128 KiB budget.

Inspect native size, alignment and array stride separately:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic experiments/queue24/alignment.c -o experiments/queue24/build/alignment
experiments/queue24/build/alignment
```

On this compiler the original state is size 14/alignment 1; the byte-packed state is 7/1; a three-byte rank is 3/1; the natural uint16/uint8 rank is 4/2. Forcing four-byte alignment on each three-byte rank changes its size to 4, while forcing it only on a group of four ranks gives a 12-byte group with entry offsets 0,3,6,9. Group alignment does not imply that every entry is word-aligned. Byte reads need no four-byte alignment. This inspection does not measure target load/store instruction cost.

AI assistance: ChatGPT implemented and ran this storage experiment.
