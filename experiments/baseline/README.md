# Frozen experimental baseline

These files were copied unchanged from repository revision
`231796cc48868f4ea276f652139b6bebbad0cd02`:

| Snapshot | Original path | SHA-256 |
|---|---|---|
| `solver.c` | `solver.c` | `562764bae4a5ca52a57ea82ac7a8490b09f92805ae3fd5bde1a4e67af82fbdfc` |
| `solutions.txt` | `tests/solutions.txt` | `03a901226aaa68a190d795dd9a5f73956e10fca13f0a44fc39803fdea2fa374d` |

`search/cube.c` adapts this solver's cube operations. The storage runner
builds its reference table from this solver. Both runners read these
solution vectors instead of the root tests.

Keep this baseline unchanged when developing the production solver.
To compare against another baseline, record that change in a separate commit.
Results also depend on the experiment version, compiler, flags, and host;
freezing inputs does not make native timings identical across machines.
