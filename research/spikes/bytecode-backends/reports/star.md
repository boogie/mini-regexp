# star

A greedy SPLIT–CHAR–JUMP loop becomes CHAR_STAR. Its VM still pushes one complete capture snapshot per choice, including the final failed iteration. Plus uses CHAR followed by CHAR_STAR. Lazy, grouped and general-class repeats retain the original instructions.

## Measured sizes

- Cortex-M4: 702 B object flash; 704 B linked flash.
- Cortex-M0+: 666 B object flash; 688 B linked flash (including required libgcc helper).
- 3587 distinct original corpus bytecode programs: 261247 → 225099 B (13.84% saved).
- On-device C compiler + optional repacker + VM: 3526 B; linked 3530 B. C output parity: 3309/3309.

## Tests

ASan/UBSan: 7197 differential cases, 0 semantic mismatches, 3 bounded-budget differences. Every compared successful match checks all capture slots. Resource differences are rerun with sufficient limits and must then agree.

## Concrete programs

| Pattern (search enabled) | baseline | star |
|---|---:|---:|
| "" | 14 | 14 |
| "a" | 18 | 16 |
| "abc" | 26 | 20 |
| "[a-z]+" | 28 | 28 |
| "a*" | 24 | 16 |
| "a+" | 28 | 18 |
| "a*?" | 27 | 25 |
| "a{1,3}" | 32 | 26 |
| "(a&#124;ab)+" | 64 | 52 |
| "\\bfoo\\b" | 28 | 22 |
| "^([a-z]+)=([0-9]+)$" | 56 | 54 |
| "abc&#124;xbc" | 44 | 32 |
| "(?:abc&#124;xbc)+" | 80 | 56 |
| "^a{100}$" | 416 | 216 |
| "é" | 20 | 20 |
| "[a-z]" | 18 | 18 |
| "[aaab]" | 18 | 18 |
| "[^a]" | 18 | 18 |
| "\\W" | 24 | 24 |
| "[a-zA-Z_]" | 22 | 22 |
| "(?:a&#124;b)*c" | 38 | 32 |

Every program's full emitted hex, including non-search samples, is in `../bytecode_samples.json`. These sizes come from assembled byte arrays executed by the C VM, not an opcode-count estimate.

## Reproduction

```sh
node ../run.js
node ../report.js
```

VM flags (in addition to the protocol flags in results.json):

```text
-DENABLE_CHAR=1 -DENABLE_SHORT=0 -DENABLE_STAR=1 -DENABLE_RANGE=0 -DENABLE_ESCAPE=0 -DSHARED_BRANCH=0
```
