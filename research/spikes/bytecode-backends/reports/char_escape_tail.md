# char_escape_tail

CHAR, escaped variable-width branches and graph sharing. This is the preferred measured option under the 600-byte VM ceiling.

## Measured sizes

- Cortex-M4: 584 B object flash; 584 B linked flash.
- Cortex-M0+: 576 B object flash; 596 B linked flash (including required libgcc helper).
- 3587 distinct original corpus bytecode programs: 261247 → 188867 B (27.71% saved).
- This exact combination has no on-device C graph compiler measurement. Its build-time JavaScript backend is executable; its cost is not claimed as zero in an embedded compiler.

## Tests

ASan/UBSan: 7197 differential cases, 0 semantic mismatches, 0 bounded-budget differences. Every compared successful match checks all capture slots. Resource differences are rerun with sufficient limits and must then agree.

## Concrete programs

| Pattern (search enabled) | baseline | char_escape_tail |
|---|---:|---:|
| "" | 14 | 12 |
| "a" | 18 | 14 |
| "abc" | 26 | 18 |
| "[a-z]+" | 28 | 22 |
| "a*" | 24 | 18 |
| "a+" | 28 | 20 |
| "a*?" | 27 | 20 |
| "a{1,3}" | 32 | 22 |
| "(a&#124;ab)+" | 64 | 32 |
| "\\bfoo\\b" | 28 | 20 |
| "^([a-z]+)=([0-9]+)$" | 56 | 44 |
| "abc&#124;xbc" | 44 | 24 |
| "(?:abc&#124;xbc)+" | 80 | 30 |
| "^a{100}$" | 416 | 218 |
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
-DENABLE_CHAR=1 -DENABLE_SHORT=1 -DENABLE_STAR=0 -DENABLE_RANGE=0 -DENABLE_ESCAPE=1 -DSHARED_BRANCH=0
```
