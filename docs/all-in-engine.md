# All-in engine (product B)

Product A is a JavaScript compiler plus a 546-byte matcher VM: patterns are compiled off-device
and only bytecode ships. Product B is for hosts that receive patterns at runtime — an embedded
scripting language, a configuration reload, a protocol field. It has **no compile step**: the
matcher interprets the pattern string directly.

Status: **experimental spike.** Not shipped, not in `src/`. Measurements below are reproducible
with the commands at the end.

## Status

Both defects this document used to open with are **fixed and verified**. Group iteration moved
into caller-provided workspace, so C-stack use no longer grows with the input, and the two
large-lower-bound quantifier patterns that used to segfault now return cleanly. The engine has
since been squeezed from 1712 to 1518 bytes across two rounds, all in portable C.

Gates on the current snapshot, all measured: size 1518 / 1586 / 2830, both corpora identical to
the reference (the single allowed failure is the deliberate `i` + `é` vs `É` Unicode-fold gap),
72/72 feature assertions, no dynamic stack frames, empty undefined-symbol list on **both** ARM
targets, clean under `-Wall -Wextra -pedantic -Wvla` on host clang and arm-none-eabi-gcc,
18.4 M ASan/UBSan cases with zero crashes, and constant stack on all five probe patterns
(`a*`, `.*`, `(a)*`, `(a|b)*`, `(\w+ )*`) from 8 to 256 characters.

## Current measurement

```
cortex-m4  (thumb2, gcc 10.3.1): text=1518 data=0 bss=0
cortex-m0+ (thumb1, gcc 10.3.1): text=1586 data=0 bss=0
x86-64     (clang 21.0.0):       text=2830 data=0
```

Concentration, Cortex-M4:

| function | bytes |
|---|---:|
| `match` | 414 |
| `atom` | 300 |
| `walk` | 190 |
| `parse_quant` | 188 |
| `group_iter` | 148 |
| `re_match` | 124 |
| `pat_char` | 108 |
| `decode` | 70 |
| `group` | 60 |
| `is_word` | 32 |

`match` and `atom` alone are 714 bytes — 44% of the engine.

Gates currently green: no dynamic stack frames, empty undefined-symbol list, zero warnings on two
toolchains across 13 feature configurations, both corpora identical to the spike baseline (the one
allowed failure is the deliberate Unicode-fold gap, `i` + `é` vs `É`), 29 M ASan/UBSan cases with
zero crashes, 72/72 hand-written feature assertions live, and 2.4 M fresh differential cases
against the untouched baseline with zero divergence.

## Size target and the real gap

The target is **approximately 1550 bytes for the product** (revised from 1400 once the format axis was measured and closed), and the product includes the host Unicode hook
layer, measured separately:

| build | Cortex-M4 | delta |
|---|---:|---:|
| engine, hooks compiled out | +40 | |
| engine, `-DRE_HOOKS` | +134 | |

So 1518 + 40…134 = **1558–1652 bytes today**, which meets the revised ~1550 target with hooks
compiled out. Any size note that omits the hook layer understates the product by 40–134 bytes.

## Feature ledger

Measured on the 1518-byte engine by compiling the feature out:

| disabled | Cortex-M4 | saving |
|---|---:|---:|
| `RE_NO_ALT` (groups + alternation) | 1056 | 462 |
| `RE_NO_CLASSES` | 1424 | 94 |
| `RE_NO_BOUNDED` (`{n,m}`) | 1426 | 92 |
| `RE_NO_UTF8` | 1444 | 74 |
| `RE_NO_WORDB` (`\b`, `\B`) | 1454 | 64 |
| `RE_NO_LAZY` | 1478 | 40 |
| `RE_NO_ICASE` | 1494 | 24 |
| `RE_NO_CAPTURES` | 1502 | 16 |
| `RE_NO_BUDGET` | 1512 | 6 |

Every individual feature is now cheaper than it was at 1634 bytes, which is the expected shape:
the size work removed shared machinery, so what remains attributable to any single feature is
smaller. Groups and alternation stay dominant at 462 bytes — 30% of the engine.

## The format axis, measured and closed

The natural next idea is to put a small compiler back in: a dumber format than product A's compact
bytecode, resolved enough that the matcher stops re-scanning the pattern. If the sum of a small
compiler and a smaller matcher dipped below the direct interpreter, that would be the way to the
target. It does not. This was measured by building the variants, not estimated.

| build | compiler | matcher | **combined** |
|---|---:|---:|---:|
| no compiler — the direct interpreter | 0 | 1634 | **1634** |
| intermediate format, maximal code sharing | 584 | 1200 | 1784 |
| fully resolved format | 882 | 1014 | 1896 |
| compact bytecode (B-lite) | 2250 | 546 | 2796 |

**The curve rises monotonically from the zero-compiler endpoint; it has no interior minimum.** The
compiler always costs more (584–882 B) than it removes from the matcher (434–620 B), and the
difference is exactly the emitter and the offset stack.

The reason is the scanning ceiling. Cutting the matcher's re-scanning step by step:

| level | removed | `.text` | delta |
|---|---|---:|---:|
| L0 | baseline | 1634 | — |
| L1 | alternation search resolved to a pointer | 1604 | −30 |
| L2 | + group-end search resolved | 1572 | −62 |
| L3 | + quantifier re-read replaced by a record | 1584 | **+12** |
| L4 | + whole structural walk deleted | 1200 | moved, not removed |

Only L0→L2 *eliminates* code: **62 bytes**. Everything below that line relocates work into a
compiler rather than removing it, and L3 is actively negative — decoding a quantifier record costs
12 bytes more than testing `*p == '*'`. The `RE_NO_ALT` ledger row of 444 B is not scanning: it is
frames, capture recording and alternation semantics, of which only 62 B is search.

One sub-hypothesis did hold and is worth recording: **fusing compiler and matcher into one
translation unit lets them share the class parser, UTF-8 decoder and escape handling**, which cut a
structural compiler to 584 B against B-lite's 2250 B — a 3.8× improvement. It simply does not help
here, because the direct interpreter is *already* the maximally fused build: its parser is its
matcher. The sharing wins against B-lite, not against 1634.

## What has been ruled out

Compiler flags and constant tuning cannot close the gap. Roughly 35 measured micro-experiments
(forced `noinline` on every helper, unsigned range tests, table-driven escape translation,
alternative `decode`/`pat_char` signatures, `re_state` field ordering, loop-form rewrites) were all
neutral or worse. Forcing inlining is actively harmful: on the comparable C compiler translation
unit, `-Os` gives 2250 bytes, `-O2` gives 3852 and `-O3` gives 5228.

Replacing the matcher core with a foreign structure — the goto-driven tail recursion of Lua's
`lstrlib` — was measured and produced no gain; the existing frame chain is competitive.

## Host hook API

Case behaviour and word-character classification are the two places where an embedding host
usually already has tables the engine should not duplicate. The hook surface is deliberately two
functions:

```c
typedef struct re_unicode {
    unsigned (*other_case)(unsigned cp);  /* other-case twin of cp, or cp if none */
    int (*is_word)(unsigned cp);          /* non-zero for \w, \W, \b, \B */
} re_unicode;

int re_match_u(const char *pat, const char *text, unsigned flags,
               int *caps, int ncaps, const re_unicode *uni);
```

A NULL field falls back to the ASCII default, and plain `re_match()` stays available. One
`other_case` is enough — and is more correct than a `toupper`/`tolower` pair — because it is the
primitive both call sites need: a literal matches when `a == b || other_case(a) == b`, and a range
matches when `lo <= c <= hi || lo <= other_case(c) <= hi`. That covers the 1:1 case pairs, which is
the whole of simple folding. Hosts that only expose `toupper`/`tolower` can wrap them.

Enabling hooks also makes `\b` code-point-based rather than byte-based, which requires stepping
back over UTF-8 continuation bytes; that cost is inside the +134.

Multi-code-point folds (`ß` → `ss`) stay out of scope, as in product A.

## Reproduce

```
tools/measure.sh <dir>/re.c -I<dir>

cc -std=c99 -Wall -Wextra -O2 -I<dir> -o <dir>/drv <dir>/driver.c <dir>/re.c
python3 tools/difftest.py <dir>/drv tests/corpus/corpus_wb_seed1.jsonl
python3 tools/difftest.py <dir>/drv tests/corpus/corpus_wb_seed7.jsonl

cc -std=c99 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all \
  -I<dir> -o <dir>/fz <dir>/fuzz.c <dir>/re.c && <dir>/fz 60 987654321
```

`tools/measure.sh` reports `.text` on all three targets, the per-function breakdown, `-fstack-usage`
frames and the undefined-symbol list. A `dynamic` frame or a non-empty symbol list is a gate
failure.

Expected corpus results: seed1 `pass=2093 fail=1`, seed7 `pass=3093 fail=1`, the single failure
being `flags='i' pat='é' text='É'` in both.
