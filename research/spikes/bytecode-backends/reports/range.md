# range

A nonnegated single ASCII range becomes ASCII_RANGE plus two bounds. General and negated classes remain unchanged.

## Measured sizes

- Cortex-M4: 634 B object flash; 636 B linked flash.
- Cortex-M0+: 598 B object flash; 620 B linked flash (including required libgcc helper).
- 3587 distinct original corpus bytecode programs: 261247 → 248291 B (4.96% saved).
- On-device C compiler + optional repacker + VM: 2930 B; linked 2934 B. C output parity: 3309/3309.

## Tests

ASan/UBSan: 7197 differential cases, 0 semantic mismatches, 0 bounded-budget differences. Every compared successful match checks all capture slots. Resource differences are rerun with sufficient limits and must then agree.

## Concrete programs

| Pattern (search enabled) | baseline | range |
|---|---:|---:|
| "" | 14 | 14 |
| "a" | 18 | 17 |
| "abc" | 26 | 23 |
| "[a-z]+" | 28 | 26 |
| "a*" | 24 | 23 |
| "a+" | 28 | 26 |
| "a*?" | 27 | 26 |
| "a{1,3}" | 32 | 29 |
| "(a&#124;ab)+" | 64 | 58 |
| "\\bfoo\\b" | 28 | 25 |
| "^([a-z]+)=([0-9]+)$" | 56 | 51 |
| "abc&#124;xbc" | 44 | 38 |
| "(?:abc&#124;xbc)+" | 80 | 68 |
| "^a{100}$" | 416 | 316 |
| "é" | 20 | 20 |
| "[a-z]" | 18 | 17 |
| "[aaab]" | 18 | 17 |
| "[^a]" | 18 | 18 |
| "\\W" | 24 | 24 |
| "[a-zA-Z_]" | 22 | 22 |
| "(?:a&#124;b)*c" | 38 | 35 |

Every program's full emitted hex, including non-search samples, is in `../bytecode_samples.json`. These sizes come from assembled byte arrays executed by the C VM, not an opcode-count estimate.

## Reproduction

```sh
node ../run.js
node ../report.js
```

VM flags (in addition to the protocol flags in results.json):

```text
-DENABLE_CHAR=0 -DENABLE_SHORT=0 -DENABLE_STAR=0 -DENABLE_RANGE=1 -DENABLE_ESCAPE=0 -DSHARED_BRANCH=0
```
