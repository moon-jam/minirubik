# Native experiments

Run commands from the repository root. These programs compare storage and
search choices on the host; they do not provide RV32I instruction counts.
Both experiments use the frozen solver and test vectors in `baseline/`.
Changes to the root solver or tests do not change this experimental baseline.
Its source revision and checksums are recorded in [`baseline/README.md`](baseline/README.md).

- `baseline/`: preserve the original solver and supplied solution vectors.
- `queue24/`: compare full-BFS queue packing and cubie packing; inspect
  structure size, alignment, and array stride.
- `search/`: compare eight bound configurations using one IDA* core.

Each directory keeps generated binaries and results in its ignored `build/`.

## Storage experiment

```sh
python3 experiments/queue24/run.py --verify-only
python3 experiments/queue24/run.py
```

The first command validates every configuration. The second also performs
five native timing and host-memory runs per configuration. Details are in
[`queue24/README.md`](queue24/README.md).

## Search experiment

| File | Responsibility |
|---|---|
| `search/compare.c` | Parse arguments, select inputs, verify answers, and report results |
| `search/profile.c` | Separate setup/search timing and logical operation counts for modes 1 and 7 |
| `search/cube.c` | Adapt the baseline's move, rank, parse, and inverse operations |
| `search/tables.c` | Generate and validate transitions, distances, classes, and perimeters |
| `search/bounds.c` | Shared packed lookups, subgroup-distance recovery, and perimeter lookup |
| `search/ida_star.c` | Search by increasing bounds, prune, record paths, and replay answers |
| `search/variants.c` | Register stable mode numbers |
| `search/variants/*.c` | Define each candidate's bound, table bytes, and additional search features |

These are bound and storage variants of IDA*, not eight different search
algorithms. Shared search rules make their work counts comparable.

| Mode | Variant file | Configuration |
|---:|---|---|
| 0 | `separate.c` | Maximum of permutation and orientation distances |
| 1 | `half4.c` | Four-bit half-turn distance plus permutation distance |
| 2 | `perimeter5.c` | Separate bounds with a radius-five perimeter |
| 3 | `half4_perimeter4.c` | Half-turn bound with a radius-four perimeter |
| 4 | `half4_inverse.c` | Half-turn bound with inverse-state lookup |
| 5 | `half_merge4.c` | Minimum of each four-entry half-turn block |
| 6 | `half_merge2.c` | Minimum of each two-entry half-turn block |
| 7 | `half_mod3.c` | Exact half-turn distances recovered from two-bit remainders |

```sh
make -C experiments/search list
make -C experiments/search check
make -C experiments/search compare
make -C experiments/search profile-storage
make -C experiments/search verify-all
```

- `list`: show mode names without preparing tables.
- `check`: test the same 64 distance-11 inputs for all eight modes.
- `compare`: test all 2,644 distance-11 inputs three times per mode, rotating
  execution order and starting a fresh process for each run.
- `profile-storage`: compare four-bit (mode 1) and two-bit (mode 7) exact
  subgroup distances, with separate operation-count and timing builds.
- `verify-all`: check mode 7 on all 3,674,160 legal inputs.

For a single configuration:

```sh
make -C experiments/search
(cd experiments && ./search/build/search-eval 7 64)
```

Arguments are `MODE [COUNT|all]`. Omitting `COUNT` tests all 2,644 hard inputs;
`all` checks every legal input. Existing solution vectors and the designated
input `21345671111111` are also checked by the hard-input comparison.

The runner checks solution lengths against a full host-only BFS oracle and
replays every returned path. Preparation checks bounds over every state,
coset transitions, packed entries, and modulo-three recovery. Timing excludes
preparation and final sorting, and includes answer checking during search.

`generated` counts child attempts, including pruned and repeated states.
`expanded` counts states that produce children. `table_bytes` counts retained
tables for the candidate, excluding frames and host-only construction data;
the experiment process prepares all candidates and uses more memory.
Reaching the per-query attempt limit is a failed run, not a verified solution.

Native timing includes the shared modular runner and bound dispatch. Compare
variants within the same build; these timings do not establish target speed.

### Four-bit versus two-bit profiling

The same 2,644 distance-11 inputs are split into two phases:

- **Setup:** initialize each query, obtain the root heuristic, and choose the
  first search bound. This includes two-bit root-distance reconstruction.
- **Search:** run the bounded depth-first passes from the prepared roots.
  This includes child-distance recovery for the two-bit version.

Table construction, allocation, warm-up, counter resets, and solution replay
are outside the timed phases. Both phases use batch-level clocks rather than
one clock per query. The timing build repeats each batch 32 times after an
untimed full-batch warm-up; three fresh processes per mode run in alternating
order. Report the median time per batch, dividing by 2,644 for a per-input
average. These are warm native batches, not cold-cache measurements or target
instruction counts. Setup also includes clearing the existing result records;
the batched runner is not identical to the original interleaved solve loop.

The separate `PROFILE_OPERATIONS` build counts logical table-entry reads,
distance updates, and root-descent steps, separately for setup and search.
These are source-level operations, not physical loads, cache misses, or
retired instructions. A transition candidate reads one permutation entry
and one orientation entry. Subgroup lookups also read a permutation-to-class
entry; the two-bit version additionally reads a parent's residue when starting
each face's candidates. The four-bit version reads a full subgroup distance
at each bound evaluation; the two-bit version carries and updates it.
Replay and host preparation contribute no operation counts. The profiler
checks transition/update counts against generated children and root-descent
steps against the reconstructed distance.

Only the counting build contains these additional operation counters. The
timing build retains the existing search-work counters used by the shared
core, including the attempt cap. Files are saved as `profile-count-MODE.json`
and `profile-time-MODE-run-N.json` in `search/build/`. All returned answers are
checked against the exact BFS reference and replayed outside timing. Equal
attempt counts establish equal search work, while these additional measures
show lookup and reconstruction overhead; RV32I costs require target runs.
