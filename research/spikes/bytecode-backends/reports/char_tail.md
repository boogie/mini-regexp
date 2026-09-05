# char_tail

CHAR and graph sharing, retaining the original long-branch encoding.

## Measured sizes

- Cortex-M4: 562 B object flash; 564 B linked flash.
- Cortex-M0+: 558 B object flash; 580 B linked flash (including required libgcc helper).
- 3587 distinct original corpus bytecode programs: 261247 → 218950 B (16.19% saved).
- This exact combination has no on-device C graph compiler measurement. Its build-time JavaScript backend is executable; its cost is not claimed as zero in an embedded compiler.

## Tests

ASan/UBSan: 7197 differential cases, 0 semantic mismatches, 0 bounded-budget differences. Every compared successful match checks all capture slots. Resource differences are rerun with sufficient limits and must then agree.

## Concrete programs

| Pattern (search enabled) | baseline | char_tail |
|---|---:|---:|
| "" | 14 | 14 |
| "a" | 18 | 16 |
| "abc" | 26 | 20 |
| "[a-z]+" | 28 | 27 |
| "a*" | 24 | 22 |
| "a+" | 28 | 24 |
| "a*?" | 27 | 25 |
| "a{1,3}" | 32 | 26 |
| "(a&#124;ab)+" | 64 | 39 |
| "\\bfoo\\b" | 28 | 22 |
| "^([a-z]+)=([0-9]+)$" | 56 | 52 |
| "abc&#124;xbc" | 44 | 28 |
| "(?:abc&#124;xbc)+" | 80 | 37 |
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
-DENABLE_CHAR=1 -DENABLE_SHORT=0 -DENABLE_STAR=0 -DENABLE_RANGE=0 -DENABLE_ESCAPE=0 -DSHARED_BRANCH=0
```
