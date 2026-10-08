"""Native storage comparison; not Ripes instruction measurements."""
import hashlib
import json
from pathlib import Path
import platform
import re
import statistics
import subprocess
import sys

root = Path(__file__).resolve().parent
build = root / "build"
build.mkdir(exist_ok=True)
layouts = [(0, 0, "uint32"), (1, 0, "bytes3"), (2, 0, "packed16_8"),
           (0, 1, "nibbles_uint32"), (1, 1, "nibbles_bytes3")]
compiler = subprocess.check_output(["cc", "--version"], text=True)
# Build an independent, unchanged upstream oracle for the full move table.
reference_source = build / "reference.c"
reference_source.write_text('''#define main upstream_cli_main
#define STATE_LAYOUT 0
#define QUEUE_LAYOUT 0
#include "../../baseline/solver.c"
#undef main
int main(void) {
    uint8_t diameter;
    uint8_t *table = build_table(&diameter);
    if (!table || diameter != 11) return 1;
    int ok = fwrite(table, 1, STATES, stdout) == STATES;
    free(table);
    return !ok || output_failed();
}
''')
subprocess.run(["cc", "-O3", "-std=c99", str(reference_source),
                "-o", str(build / "reference")], check=True)
reference = subprocess.check_output([str(build / "reference")])
if hashlib.sha256(reference).hexdigest() != "ee9061e8e6db727b7c2c78b3d6b73d0d85a5be552f1980054dafa5f594c0c345":
    raise RuntimeError("Baseline table differs from the verified upstream table")
for layout, state_layout, name in layouts:
    flags = ["-O3", "-std=c99", "-Wall", "-Wextra", "-Wpedantic",
             f"-DQUEUE_LAYOUT={layout}", f"-DSTATE_LAYOUT={state_layout}"]
    for source, suffix in [(root / "solver.c", "solver"),
                           (root / "bench.c", "bench")]:
        subprocess.run(["cc", *flags, str(source), "-o",
                        str(build / f"{name}-{suffix}")], check=True)
    data = subprocess.check_output([str(build / f"{name}-bench"), "--export"])
    if len(data) != 3674160:
        raise RuntimeError("Incomplete table")
    if data != reference:
        raise RuntimeError(f"Full move table differs for {name}")
    # Cover all supplied solution vectors, not just the hardest sample.
    for line in (root.parent / "baseline/solutions.txt").read_text().splitlines():
        if not line or line.startswith("#"):
            continue
        state, expected = line.split("|", 1)
        actual = subprocess.check_output([str(build / f"{name}-solver"), state], text=True)
        if actual != expected + "\n":
            raise RuntimeError(f"Solution differs for {name}: {state}")
    for invalid in ["", "1234567111111", "123456711111111",
                    "12345671111112", "11345671111111", "82345671111111",
                    "12345674111111"]:
        result = subprocess.run([str(build / f"{name}-solver"), invalid],
                                capture_output=True)
        if result.returncode != 2 or result.stdout:
            raise RuntimeError(f"Invalid input accepted for {name}: {invalid}")
    print(f"{name}: full table and all supplied solutions match", flush=True)

if "--verify-only" in sys.argv[1:]:
    sys.exit(0)

records = []
for iteration in range(5):
    order = layouts[iteration % len(layouts):] + layouts[:iteration % len(layouts)]
    for layout, state_layout, name in order:
        result = subprocess.run(["/usr/bin/time", "-l", str(build / f"{name}-bench"),
                                 "--benchmark"], check=True, capture_output=True, text=True)
        (build / f"{name}-{iteration + 1}.json").write_text(result.stdout)
        (build / f"{name}-{iteration + 1}.host.txt").write_text(result.stderr)
        record = json.loads(result.stdout)
        record["name"] = name
        record["iteration"] = iteration + 1
        record["host_rss_bytes"] = int(re.search(r"(\d+)\s+maximum resident set size",
                                               result.stderr).group(1))
        records.append(record)

summary = {"execution": "host-only experiment; not Ripes",
           "compiler": compiler, "platform": platform.platform(),
           "full_table_sha256": hashlib.sha256(reference).hexdigest(),
           "repetitions": 5, "layouts": {}}
for layout, state_layout, name in layouts:
    subset = [r for r in records if r["name"] == name]
    summary["layouts"][name] = dict(
        state_bytes=subset[0]["state_bytes"],
        entry_bytes=subset[0]["entry_bytes"], queue_bytes=subset[0]["queue_bytes"],
        natural_fields_bytes=subset[0]["natural_fields_bytes"],
        median_build_ms=statistics.median(r["build_ms"] for r in subset),
        median_host_rss_bytes=statistics.median(r["host_rss_bytes"] for r in subset),
        min_build_ms=min(r["build_ms"] for r in subset),
        max_build_ms=max(r["build_ms"] for r in subset))
(build / "results.json").write_text(json.dumps(dict(summary=summary, raw=records), indent=2))
print(json.dumps(summary, indent=2))
