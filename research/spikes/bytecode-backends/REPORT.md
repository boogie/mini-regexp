# Measured bytecode backend experiments

The four requested steps are implemented: CHAR, short/long branches, fused atom repetition and graph-based tail sharing. All work is isolated in this directory, with a frozen snapshot of the original VM, compilers and corpora. The original src/ engine and its format are unchanged.

## Result

The measured choice under the 600-byte Cortex-M4 VM ceiling is **CHAR + escaped branches + tail sharing: 584 B VM and 27.71% smaller bytecode** on distinct corpus programs. Tail sharing alone saves 8.37% and runs on the existing 546 B VM. These numbers do not solve the 1400 B all-in engine target: on-device compiler costs remain much larger.

## Measurement protocol

Toolchain: arm-none-eabi-gcc (GNU Arm Embedded Toolchain 10.3-2021.10) 10.3.1 20210824 (release). The exact flags and snapshot SHA-256 hashes are in results.json.

Object flash means .text + .rodata + initialized data, consistent with tools/measure.sh. Linked flash includes alignment and any pulled-in libgcc routines, with no libc or startup. M0+ needs __gnu_thumb1_case_uqi in several builds; omitting it would undercount the image. The linked images have no unresolved symbols. Pattern bytecode is additional to VM flash. JavaScript build-tool size is not target flash.

## All executable VM variants

| Variant | M4 object B | M4 linked B | M0+ linked B | Corpus bytecode B | Saved |
|---|---:|---:|---:|---:|---:|
| [baseline](reports/baseline.md) | 546 | 548 | 568 | 261247 | 0.00% |
| [char](reports/char.md) | 562 | 564 | 580 | 237339 | 9.15% |
| [short](reports/short.md) | 614 | 616 | 608 | 230536 | 11.76% |
| [char_short](reports/char_short.md) | 602 | 604 | 608 | 206401 | 20.99% |
| [star](reports/star.md) | 702 | 704 | 688 | 225099 | 13.84% |
| [tail](reports/tail.md) | 546 | 548 | 568 | 239392 | 8.37% |
| [range](reports/range.md) | 634 | 636 | 620 | 248291 | 4.96% |
| [all](reports/all.md) | 690 | 692 | 712 | 181720 | 30.44% |
| [all_range](reports/all_range.md) | 718 | 720 | 744 | 180852 | 30.77% |
| [short_shared](reports/short_shared.md) | 618 | 620 | 596 | 230536 | 11.76% |
| [char_short_shared](reports/char_short_shared.md) | 606 | 608 | 596 | 206401 | 20.99% |
| [all_shared](reports/all_shared.md) | 686 | 688 | 700 | 181720 | 30.44% |
| [escape](reports/escape.md) | 564 | 564 | 580 | 231492 | 11.39% |
| [char_escape](reports/char_escape.md) | 584 | 584 | 596 | 207128 | 20.72% |
| [char_tail](reports/char_tail.md) | 562 | 564 | 580 | 218950 | 16.19% |
| [char_escape_tail](reports/char_escape_tail.md) | 584 | 584 | 596 | 188867 | 27.71% |

The corpus totals use 3587 distinct original bytecode programs (deduplicated by original bytes), not frequency-weighted input cases. The baseline sum is 261247 B. Per-seed weighted totals are also preserved in results.json.

## Program sizes

| Pattern (search enabled) | baseline | char | short | star | tail | char_escape_tail |
|---|---:|---:|---:|---:|---:|---:|
| "" | 14 | 14 | 12 | 14 | 14 | 12 |
| "a" | 18 | 16 | 16 | 16 | 18 | 14 |
| "abc" | 26 | 20 | 24 | 20 | 26 | 18 |
| "[a-z]+" | 28 | 28 | 24 | 28 | 27 | 22 |
| "a*" | 24 | 22 | 20 | 16 | 24 | 18 |
| "a+" | 28 | 24 | 24 | 18 | 27 | 20 |
| "a*?" | 27 | 25 | 22 | 25 | 27 | 20 |
| "a{1,3}" | 32 | 26 | 28 | 26 | 32 | 22 |
| "(a&#124;ab)+" | 64 | 52 | 56 | 52 | 45 | 32 |
| "\\bfoo\\b" | 28 | 22 | 26 | 22 | 28 | 20 |
| "^([a-z]+)=([0-9]+)$" | 56 | 54 | 50 | 54 | 54 | 44 |
| "abc&#124;xbc" | 44 | 32 | 40 | 32 | 36 | 24 |
| "(?:abc&#124;xbc)+" | 80 | 56 | 72 | 56 | 45 | 30 |
| "^a{100}$" | 416 | 216 | 416 | 216 | 416 | 218 |
| "é" | 20 | 20 | 18 | 20 | 20 | 18 |
| "[a-z]" | 18 | 18 | 16 | 18 | 18 | 16 |
| "[aaab]" | 18 | 18 | 16 | 18 | 18 | 16 |
| "[^a]" | 18 | 18 | 16 | 18 | 18 | 16 |
| "\\W" | 24 | 24 | 22 | 24 | 24 | 22 |
| "[a-zA-Z_]" | 22 | 22 | 20 | 22 | 22 | 20 |
| "(?:a&#124;b)*c" | 38 | 32 | 32 | 32 | 38 | 26 |

## On-device C compiler cost

These are compiled C implementations, not estimates. The compact-atom compiler emits CHAR/RANGE directly. Branch relaxation and star fusion use a separate C repacking pass. This pass is a cost probe, not a size-optimized parser redesign. Graph sharing currently runs only in the JavaScript backend.

| C backend | Parser/emitter B | Compiler + repacker + VM B | Linked B | Byte-for-byte parity |
|---|---:|---:|---:|---:|
| baseline | 2250 | 2796 | 2798 | 3309/3309 |
| char | 2292 | 2854 | 2858 | 3309/3309 |
| range | 2296 | 2930 | 2934 | 3309/3309 |
| char_range | 2318 | 2964 | 2966 | 3309/3309 |
| short | 2250 | 3378 | 3380 | 3309/3309 |
| char_short | 2292 | 3408 | 3412 | 3309/3309 |
| star | 2292 | 3526 | 3530 | 3309/3309 |
| char_short_star | 2292 | 3650 | 3654 | 3309/3309 |
| escape | 2250 | 3384 | 3384 | 3309/3309 |
| char_escape | 2292 | 3446 | 3448 | 3309/3309 |

C parity covers all 3309 distinct successfully JS-compiled non-i (pattern, flags) pairs in the frozen corpora. C Unicode-i compilation remains unsupported, as in the original compiler; the VM/backend tests retain Unicode-i expansion from JavaScript. The previous conversation's 3628/3628 claim was not reproduced by this dataset and is not used here.

Compiler scratch remains 1016 B. The repacker additionally requires a caller-owned array of 24-byte records, one per input instruction (allocating input_length records is a conservative bound), plus separate input/output bytecode buffers. All buffers must be disjoint; re_repack accepts trusted compiler output. The compiler may temporarily emit more bytes than the final program, e.g. before removing a {0} body. C parity tests use a 64 KiB output buffer; the final bytecode length alone is not a sufficient compiler capacity guarantee.

## Validation and limits

- 7197 cases per VM build, including all 4839 accepted corpus rows, extra generated combinations, long relocations, malformed UTF-8 and resource boundaries.
- All 16 builds have zero semantic mismatches under ASan/UBSan; all successful results compare every capture slot with the snapshot VM.
- Fused-star builds differ on three deliberately small instruction budgets: fusion executes fewer dispatch steps. All three agree when rerun with sufficient budget. The instruction budget remains enforced, but is not an invariant amount of logical regex work across representations.
- The original compiler rejects 367 corpus rows; their names, patterns and errors are retained in results.json. They are not classified as successful VM tests.
- C repacker tests exercise zero output capacity, insufficient scratch, too-small output and invalid jump targets. The public trust boundary still excludes arbitrary untrusted bytecode.
- Host execution validates the C implementations; M4/M0+ figures are cross-compiled and linked. No board timing or hardware execution claim is made.

## Deployment and further directions

1. **Tail sharing first:** it requires no VM change and saves 21855 B across the distinct-program corpus. A precise result comes from the actual fully relocated candidate; longer results fall back to the original program.
2. **CHAR + escape + tail for a new format:** +38 B of VM object flash buys 72380 B of aggregate bytecode saving. Corpus-average break-even is two distinct patterns, but application-specific patterns determine the real result. The escaped branch format is incompatible with the old operand encoding, so its compiler output and VM must ship together.
3. **Keep fused CHAR_STAR optional:** its extra dispatch and snapshot code exceeds the 600 B VM ceiling. The 686 B shared-dispatch combination saves 30.44%; that is useful only when pattern storage outweighs the larger VM.
4. **Next bytecode target: counted repetition.** ^a{100}$ still takes 218 B in the selected format versus 416 B originally. A counted-repeat instruction could remove this linear bytecode expansion; it must account for counters and capture restoration before a size claim is justified.
5. **Next compiler target: direct compact emission.** The measured C repacker adds substantial flash and scratch. A compiler emitting the selected representation directly should be compared against the measured 3446 B char_escape toolchain, with Unicode folding cost still explicit. No unmeasured savings are claimed.
6. **Tail layout improvements:** the current pass merges equivalent continuations and then restores fallthrough with jumps. A layout optimized for frequently shared successors might remove more jumps without expanding the VM. The current fully relocated length gate is the acceptance criterion.

Assembly hot paths, tagged workspace records and native post-link optimization do not change regex bytecode size. They are not assigned invented bytecode savings. A pattern-specific native-code generator would replace bytecode with machine code; calling its bytecode 0 B is not a useful total-size result, and no working native generator is claimed by these experiments.

## Reproduce

Run from this directory:

```sh
node run.js
node report.js
```

run.js compiles and links both ARM targets, builds sanitizer-enabled host differential runners, runs all VM and C-compiler comparisons, and writes results.json and bytecode_samples.json. report.js regenerates this report and one report per variant. Build intermediates stay in ignored build/. A complete sample hex dump is provided for every variant and pattern.
