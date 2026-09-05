# All-in demoscene experiment

This directory is an isolated research area. It does not replace the current Product B source in
`research/spikes/direct/`.

## Current reference

The current bounded-stack direct interpreter measures:

| target | `.text` |
|---|---:|
| Cortex-M4 / Thumb-2 | **1518 B** |
| Cortex-M0+ / Thumb-1 | **1586 B** |
| x86-64 | **2830 B** |

It has no dynamic stack frames and no undefined symbols on either Arm target. The full product
with the optional host hook layer is 1558–1652 B, depending on configuration.

## Completed experiments

The isolated `snapshot/` is the earlier 1712-byte bounded-stack version. A unified group/quantifier
candidate reduced that snapshot to **1648 B** on Cortex-M4, but it is not competitive with the
current 1518-byte source. It passed 72/72 feature assertions, both differential corpora with the
single documented Unicode-fold exception, and 20,000 differential fuzz cases. It remains a
rejected historical experiment rather than a product candidate.

The following compiler switches were measured against the current 1518-byte source; none changed
the Cortex-M4 size:

`-fno-jump-tables`, `-fno-tree-switch-conversion`, `-fno-ipa-icf`, `-fno-tree-sra`,
`-fno-schedule-insns`, `-fshort-enums`, `-fno-merge-constants`, and `-fno-plt`.

`-fno-cse-follow-jumps` grew the result to 1528 B; `-fno-reorder-blocks` grew it to 1562 B.

## Conclusion

The candidate structural merge and the remaining low-risk compiler switches do not reach 1400 B
without removing a feature or changing the execution model. The current 1518-byte direct
interpreter remains the best validated all-in result in this repository. Further progress needs a
new matcher algorithm, not another instruction-level or flag-level pass.
