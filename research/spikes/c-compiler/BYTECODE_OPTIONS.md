# Bytecode backend measurements

The earlier size models in this file are superseded by executable backend experiments:

- [Full measured report](../bytecode-backends/REPORT.md)
- [Per-variant reports](../bytecode-backends/reports/)
- [Complete emitted bytecode samples](../bytecode-backends/bytecode_samples.json)
- [Machine-readable measurements and test results](../bytecode-backends/results.json)

The original matcher remains 546 B on Cortex-M4. The measured CHAR + escaped-branch VM is
584 B; adding build-time graph sharing saves 27.71% of bytecode across 3587 distinct corpus
programs. Graph sharing alone saves 8.37% and uses the existing VM unchanged.

The on-device compiler remains the larger cost: baseline compiler + VM is 2796 B, while the
implemented CHAR + escaped-branch C compiler/repacker + VM is 3446 B. That C toolchain does
not include graph sharing or Unicode-i compilation. The JavaScript backend preserves the
original Unicode-i expansion.

The reports distinguish VM machine code, regex bytecode, compiler flash, linked runtime
dependencies and scratch memory. Earlier hand-counted figures and the unverified 3628/3628
parity claim must not be used as measurements; this run checks all 3309 distinct accepted
non-i pattern/flags pairs in its frozen corpora.

Reproduce from `research/spikes/bytecode-backends` with:

```sh
node run.js
node report.js
```
