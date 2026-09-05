# char_short_shared

Same bytes as char_short, with a shared C branch dispatch path.

## Measured sizes

- Cortex-M4: 606 B object flash; 608 B linked flash.
- Cortex-M0+: 574 B object flash; 596 B linked flash (including required libgcc helper).
- 3587 distinct original corpus bytecode programs: 261247 → 206401 B (20.99% saved).
- This exact combination has no on-device C graph compiler measurement. Its build-time JavaScript backend is executable; its cost is not claimed as zero in an embedded compiler.

## Tests

ASan/UBSan: 7197 differential cases, 0 semantic mismatches, 0 bounded-budget differences. Every compared successful match checks all capture slots. Resource differences are rerun with sufficient limits and must then agree.

## Concrete programs

| Pattern (search enabled) | baseline | char_short_shared |
|---|---:|---:|
| "" | 14 | 12 |
| "a" | 18 | 14 |
| "abc" | 26 | 18 |
| "[a-z]+" | 28 | 24 |
| "a*" | 24 | 18 |
| "a+" | 28 | 20 |
| "a*?" | 27 | 20 |
| "a{1,3}" | 32 | 22 |
| "(a&#124;ab)+" | 64 | 44 |
| "\\bfoo\\b" | 28 | 20 |
| "^([a-z]+)=([0-9]+)$" | 56 | 48 |
| "abc&#124;xbc" | 44 | 28 |
| "(?:abc&#124;xbc)+" | 80 | 48 |
| "^a{100}$" | 416 | 216 |
| "é" | 20 | 18 |
| "[a-z]" | 18 | 16 |
| "[aaab]" | 18 | 16 |
| "[^a]" | 18 | 16 |
| "\\W" | 24 | 22 |
| "[a-zA-Z_]" | 22 | 20 |
| "(?:a&#124;b)*c" | 38 | 26 |

Every program's full emitted hex, including non-search samples, is in `../bytecode_samples.json`. These sizes come from assembled byte arrays executed by the C VM, not an opcode-count estimate.

## Reproduction

```sh
node ../run.js
node ../report.js
```

VM flags (in addition to the protocol flags in results.json):

```text
-DENABLE_CHAR=1 -DENABLE_SHORT=1 -DENABLE_STAR=0 -DENABLE_RANGE=0 -DENABLE_ESCAPE=0 -DSHARED_BRANCH=1
```
