# escape

SPLIT and JUMP each use one signed operand byte in [-127,127]. Operand 0x80 introduces a signed 16-bit displacement. Long instructions therefore cost four bytes; short ones cost two. This format requires its dedicated VM build.

## Measured sizes

- Cortex-M4: 564 B object flash; 564 B linked flash.
- Cortex-M0+: 560 B object flash; 580 B linked flash (including required libgcc helper).
- 3587 distinct original corpus bytecode programs: 261247 → 231492 B (11.39% saved).
- On-device C compiler + optional repacker + VM: 3384 B; linked 3384 B. C output parity: 3309/3309.

## Tests

ASan/UBSan: 7197 differential cases, 0 semantic mismatches, 0 bounded-budget differences. Every compared successful match checks all capture slots. Resource differences are rerun with sufficient limits and must then agree.

## Concrete programs

| Pattern (search enabled) | baseline | escape |
|---|---:|---:|
| "" | 14 | 12 |
| "a" | 18 | 16 |
| "abc" | 26 | 24 |
| "[a-z]+" | 28 | 24 |
| "a*" | 24 | 20 |
| "a+" | 28 | 24 |
| "a*?" | 27 | 22 |
| "a{1,3}" | 32 | 28 |
| "(a&#124;ab)+" | 64 | 56 |
| "\\bfoo\\b" | 28 | 26 |
| "^([a-z]+)=([0-9]+)$" | 56 | 50 |
| "abc&#124;xbc" | 44 | 40 |
| "(?:abc&#124;xbc)+" | 80 | 72 |
| "^a{100}$" | 416 | 418 |
| "é" | 20 | 18 |
| "[a-z]" | 18 | 16 |
| "[aaab]" | 18 | 16 |
| "[^a]" | 18 | 16 |
| "\\W" | 24 | 22 |
| "[a-zA-Z_]" | 22 | 20 |
| "(?:a&#124;b)*c" | 38 | 32 |

Every program's full emitted hex, including non-search samples, is in `../bytecode_samples.json`. These sizes come from assembled byte arrays executed by the C VM, not an opcode-count estimate.

## Reproduction

```sh
node ../run.js
node ../report.js
```

VM flags (in addition to the protocol flags in results.json):

```text
-DENABLE_CHAR=0 -DENABLE_SHORT=1 -DENABLE_STAR=0 -DENABLE_RANGE=0 -DENABLE_ESCAPE=1 -DSHARED_BRANCH=0
```
