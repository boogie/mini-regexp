# tail

Jump threading, unreachable-node removal and graph partition refinement merge equivalent continuations. Equivalence includes opcode operands, capture slots and ordered successors. Serialization adds explicit jumps when necessary; the result is accepted only if the fully relocated program is smaller. The existing VM executes it unchanged.

## Measured sizes

- Cortex-M4: 546 B object flash; 548 B linked flash.
- Cortex-M0+: 546 B object flash; 568 B linked flash (including required libgcc helper).
- 3587 distinct original corpus bytecode programs: 261247 → 239392 B (8.37% saved).
- This exact combination has no on-device C graph compiler measurement. Its build-time JavaScript backend is executable; its cost is not claimed as zero in an embedded compiler.

## Tests

ASan/UBSan: 7197 differential cases, 0 semantic mismatches, 0 bounded-budget differences. Every compared successful match checks all capture slots. Resource differences are rerun with sufficient limits and must then agree.

## Concrete programs

| Pattern (search enabled) | baseline | tail |
|---|---:|---:|
| "" | 14 | 14 |
| "a" | 18 | 18 |
| "abc" | 26 | 26 |
| "[a-z]+" | 28 | 27 |
| "a*" | 24 | 24 |
| "a+" | 28 | 27 |
| "a*?" | 27 | 27 |
| "a{1,3}" | 32 | 32 |
| "(a&#124;ab)+" | 64 | 45 |
| "\\bfoo\\b" | 28 | 28 |
| "^([a-z]+)=([0-9]+)$" | 56 | 54 |
| "abc&#124;xbc" | 44 | 36 |
| "(?:abc&#124;xbc)+" | 80 | 45 |
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
