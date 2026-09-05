# Research

Material behind `docs/feasibility.md` (2026-09-04). Produced by automated survey/measurement
agents; every number has its commands recorded.

- `results/measure:*.json` — per-engine measurements: Cortex-M4 / Cortex-M0+ / x86-64 `.text` at `-Os`,
  per-function breakdown, libc dependencies, RAM model, differential-test results with failure
  classification.
- `results/survey*.json` — catalog of 51 C regex engines, the embedded-language survey with
  evidence quotes, the algorithm/size-technique analysis, and the test-resource survey.
- `engines/<name>/BUILD.txt` — exact commands used for each measurement (`driver.c` = the CLI
  wrapper used for the differential test). The engine sources themselves are not tracked;
  clone them again from the URLs in the JSON files.
- `spikes/` — proof-of-concept engines. `compiler-vm/` contains the host-side JavaScript compiler
  plus the sub-600-byte Cortex-M4 VM and differential-corpus adapter; `c-compiler/` measures the
  optional on-device compiler variant.

Measurement protocol: `tools/measure.sh FILE.c`, engine translation units only, no test/debug
code, `.text` includes `.rodata`.

## Reading the BUILD.txt files

Each `engines/<name>/BUILD.txt` is a transcript of the commands actually run to fetch, build and
measure that engine, including the upstream URL and the pinned commit. Two placeholders stand in
for machine-specific paths:

- `$WORK` — a scratch directory where sources were cloned and objects built
- `$REPO` — the root of this repository

Set both and the transcripts replay:

```sh
export REPO=$PWD
export WORK=$(mktemp -d)
```

Engine sources are not vendored here; only the fetch command, the pinned commit and the
measurement output are. That keeps the repository small and makes each measurement independently
checkable against upstream.
