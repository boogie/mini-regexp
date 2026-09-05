# baseline

Unmodified instruction format; the readable experimental VM reproduces the original 546-byte M4 object.

## Measured sizes

- Cortex-M4: 546 B object flash; 548 B linked flash.
- Cortex-M0+: 546 B object flash; 568 B linked flash (including required libgcc helper).
- 3587 distinct original corpus bytecode programs: 261247 → 261247 B (0.00% saved).
- On-device C compiler + optional repacker + VM: 2796 B; linked 2798 B. C output parity: 3309/3309.

## Tests

ASan/UBSan: 7197 differential cases, 0 semantic mismatches, 0 bounded-budget differences. Every compared successful match checks all capture slots. Resource differences are rerun with sufficient limits and must then agree.

## Concrete programs

| Pattern (search enabled) | baseline | baseline |
|---|---:|---:|
| "" | 14 | 14 |
| "a" | 18 | 18 |
| "abc" | 26 | 26 |
| "[a-z]+" | 28 | 28 |
| "a*" | 24 | 24 |
| "a+" | 28 | 28 |
| "a*?" | 27 | 27 |
| "a{1,3}" | 32 | 32 |
| "(a&#124;ab)+" | 64 | 64 |
| "\\bfoo\\b" | 28 | 28 |
| "^([a-z]+)=([0-9]+)$" | 56 | 56 |
| "abc&#124;xbc" | 44 | 44 |
| "(?:abc&#124;xbc)+" | 80 | 80 |
| "^a{100}$" | 416 | 416 |
| "é" | 20 | 20 |
| "[a-z]" | 18 | 18 |
| "[aaab]" | 18 | 18 |
| "[^a]" | 18 | 18 |
| "\\W" | 24 | 24 |
| "[a-zA-Z_]" | 22 | 22 |
| "(?:a&#124;b)*c" | 38 | 38 |

Every program's full emitted hex, including non-search samples, is in `../bytecode_samples.json`. These sizes come from assembled byte arrays executed by the C VM, not an opcode-count estimate.

## Reproduction

```sh
node ../run.js
node ../report.js
```

VM flags (in addition to the protocol flags in results.json):

```text
-DENABLE_CHAR=0 -DENABLE_SHORT=0 -DENABLE_STAR=0 -DENABLE_RANGE=0 -DENABLE_ESCAPE=0 -DSHARED_BRANCH=0
```
