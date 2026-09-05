# Embedded C compiler spike

This spike tests a second product shape: compile patterns on the target, then execute the same
bytecode with `src/re.c`. The compiler has no heap, libc or mutable globals. Its output buffer and
range scratch space are caller-owned.

## Scope

The C compiler emits byte-for-byte identical programs to `tools/re_compile.js` for literals,
UTF-8 code points, dot, anchors, groups, alternation, classes, shorthand classes, greedy and lazy
quantifiers, bounded repeats, search wrapping, and the `m` and `s` flags. It rejects unbounded
repetition of nullable atoms. Unicode `i` is not implemented.

The bytecode constants are named in `src/re_bytecode.h`; compiler policy and UTF-8 constants are
named in `re_compile.h` and `re_compile.c`. The JavaScript compiler uses the corresponding named
constants rather than instruction-layout magic numbers.

## Result

On the pinned Cortex-M4/GCC 10.3 `-Os -ffreestanding` measurement:

| Component | Bytes |
|---|---:|
| Matcher VM (A) | 546 |
| C compiler only | 2250 |
| Combined compiler + matcher (B-lite) | **2796** |

The compiler needs 1016 bytes of caller scratch for 127 ranges, its output bytecode buffer, and
approximately 64 bytes plus 80 bytes per nested group/alternation level of C stack. Scratch can be
reused as matcher workspace after compilation if its lifetime permits.

All 3628 distinct non-`i` patterns in the committed corpora produce byte-for-byte identical output
from the C and JavaScript compilers.

## Unicode `i`

Matching JavaScript's current simple-fold expansion requires data, not just parser code. The
current Unicode behavior contains 1509 non-canonical code points in 698 contiguous delta runs.
A custom packed table is estimated at roughly 3.6–4.9 KB before lookup and expansion code. A
feature-equivalent B-full build would therefore likely be around 6.4–8 KB, not around 3 KB.

## Decision

B-lite makes sense as an optional artifact for runtime-supplied patterns or C-only build systems.
It does not replace the 546-byte A product for precompiled patterns. B-full only makes sense if
runtime Unicode-`i` compilation is worth several additional kilobytes; otherwise B should reject
`i` or define it as ASCII-only under a distinct API contract.

See `BUILD.txt` for reproduction commands.
