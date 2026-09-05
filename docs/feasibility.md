# mini-regexp: feasibility study

Date: 2026-09-04. Written before the project started, to answer three questions: what is available today and is a new engine justified; is it feasible in 1000-2000 bytes; how should we start.

> Aftermath: the implementation ended up as a separate JavaScript compiler plus a C VM. The current VM is 546 bytes
> on Cortex-M4, with a 600-byte hard ceiling; for the later decisions see `DECISIONS.md`.

Every size below: Cortex-M4 (Thumb-2, nRF52), `arm-none-eabi-gcc 10.3 -Os`, the engine translation unit only, `.text` together with the `.rodata` tables (= flash), without linking. The measurement script and the exact commands are in `tools/measure.sh` and `research/engines/*/BUILD.txt`.

## 1. Summary

- **Justified.** We catalogued 51 C engines and measured 12: below 3 KB no existing engine delivers the "modern basics" (alternation + groups + `{n,m}` + lazy quantifiers + UTF-8 code points + ASCII `i` flag + `\b`). Every one of them drops at least two of these, or is not leftmost-first. The engines that do the full list are 5-13 KB.
- **Feasible.** Three independent prototypes were built with the full v1 feature list. The direct pattern-interpreting backtracker passes all 2095 differential test cases in 1.9 KB (Python `re` is the oracle). Details in section 6.
- **Recommendation:** direct pattern interpretation (no compilation step, no bytecode), recursive backtracking with a step limit, Perl/Python semantics, Python `re` as the oracle, a size gate on every commit.

## 2. What is available today

### 2.1 Measured engines

| Engine | Licence | M4 bytes | Missing for v1 | Corpus (2081) |
|---|---|---|---|---|
| Kernighan/Pike TPOP matcher | book | 156 | everything; only `c . ^ $ *`, boolean | 1189* |
| kokke/tiny-regex-c | Unlicense | 1162 (+257 ctype) | `\|`, groups, `{n,m}`, lazy, UTF-8, `i`; not reentrant (280 B bss) | 1224 |
| Lua 5.4 pattern matcher (lstrlib) | MIT | 1481** | not regexp: no `\|`, no group quantifier; does not run without a host | 809 |
| re1.5 (MicroPython `re`) | BSD-3 | 1530 | `{n,m}`, UTF-8, `i`, `\b`; SIGSEGV on `(a*)*` | 982 |
| subreg | MIT | 1704 | does not backtrack (possessive), anchored only, `{n,m}`, lazy, UTF-8 | 1489 |
| zeta-zero/tiny-regex-c | Apache-2 | 1865 | groups and alternation together, capture, lazy, UTF-8, `i` | not measured |
| SLRE (Cesanta) | **GPL-2** | 1946 | `{n,m}`, `\w`, `(?:)`, UTF-8 | 1089 |
| SQLite ext/regexp.c | PD | ~2100 | capture (boolean), lazy | not measured |
| Espruino RegExp | MPL-2 | 2267 | `\|` inside a group, `{n,m}`, lazy; needs a JsVar host | not measured |
| rsc/re1 (Cox) | BSD | 2468 | classes, `^ $`, `\d\w\s`, `{n,m}`, `i`; malloc + yacc | 707 |
| Henry Spencer 1986 | own | 2751 | `(?:)`, lazy, `{n,m}`, `\d\w\s`, `i`; malloc | 1029 |
| T-Rex (Squirrel) | zlib | 2929 | lazy, `i`, UTF-8; malloc+setjmp; not leftmost-first | 628 |
| Plan 9 libregexp | MIT | 3137 (+206 UTF-8) | `{n,m}`, lazy, `\d\w\s`, `i`; **33 KB RAM** per pattern | 1105 |
| Jim Tcl jimregexp | BSD-2 | 5166 | complete, but 2.5x the budget | not measured |
| mujs regexp.c | ISC | ~5400 | complete JS subset, 2.7x the budget | not measured |
| Duktape RegExp | MIT | ~5600-6100 | complete ES5, needs a host | not measured |
| MicroQuickJS regexp | MIT | ~7400 | complete ES5, needs a host | not measured |
| musl/TRE (POSIX) | MIT | 13309 (+20 KB libc) | leftmost-longest, not Perl semantics; malloc | 1406 |
| QuickJS libregexp | MIT | 13334 (+49 KB libunicode) | UTF-16 internally, malloc | 2054 |

\* 42% of the corpus is a "no match" case, so even a dumb engine scores 800-900; of the Pike matcher's 1189, 877 are accidental agreements. The numbers are only meaningful relative to each other.
\*\* Matcher functions + search loop (the independent re-measurement corrected this from 1393 so that the calculation is the same as for the other engines); the language survey measured ~3.0 KB with all the rodata and the Lua API glue. Not a regexp language, it is here for comparison.

The 2081 cases (`corpus_seed1`, the first corpus, without `\b`; the spikes run on the later `corpus_wb_seed1`, 2095 cases, which does include `\b`): Python `re` (`re.ASCII`) is the oracle, Perl/JS/Python leftmost-first semantics, byte offsets in UTF-8 text, with group spans checked. The failure classification (missing feature / semantics / bug) is given per engine in `research/results/measure:*.json`.

### 2.2 What the catalogue shows

- Below 3 KB every engine is a Kernighan/Pike-style backtracker over a token array, and every one drops at least two v1 features. None of them is leftmost-first correct with captures.
- The VM family (re1.5, SQLite, cregex, Plan 9) handles nested groups correctly, but none of them has UTF-8 + `{n,m}` + `i` together.
- The engines that do the full list (mujs, Jim, Duktape, mquickjs, Remimu, wregex, librxvm, QuickJS, musl) are 2.5-7x the size of the 2 KB target.
- Licence blocker: SLRE GPL-2/commercial, dietlibc GPL-2, regExpEmbedded LGPL-3. The rest are MIT/BSD/zlib/PD.
- The independent re-measurement reproduced the numbers for all four selected engines exactly (the `BUILD.txt` files are sufficient), and unified the exclusion rule in two places (tiny-regex-c: the public `re_match` entry point is engine too; Lua: so is the search loop). The x86-64 `.text` is consistently 1.5-1.9x the Cortex-M4 one.
- The gap review found 32 further engines (`research/results/survey:engines.json`); the more important ones: sgregex (SGScript, 5012 B), regex_light (PicoRuby, 2465 B, ASCII, no `|`), tiny-str-match (kokke + UTF-8 + `|` + `{n,m}`, without capture), MaJerle/RegExp, CPS-RE. None of them changes the picture; the numbers for the measurable ones go into the table after the follow-up measurement.
- Several "tiny" engines' READMEs give an x86 size, often including the debug/print code; the Cortex-M4, engine-only number is consistently 30-50% smaller than what they advertise.

## 3. Embedded languages and the "too big" thesis

What they use today:

| Runtime | Regexp | Engine | Size (M4) |
|---|---|---|---|
| MicroPython | `re` module, optional | re1.5 | 1566 |
| CircuitPython | `re` module, disabled on SAMD21 | modified re1.5 copy | 1676 |
| Berry (Tasmota) | `re` module | re1.5 copy | 1564 |
| Espruino | built in, removed with SAVE_ON_FLASH | own (jswrap_regexp.c) | 2267 |
| Squirrel | built in | T-Rex | 3144 |
| Duktape | option, labelled "lowmemory" | own | ~5600 |
| JerryScript | option | own | ~13900 (MinSizeRel) |
| MicroQuickJS | built in | cut down from QuickJS | ~7400 |
| Lua / NodeMCU / eLua | Lua patterns | lstrlib | ~3000 |
| Janet | none, PEG instead | peg.c | 12541 |
| PicoRuby | `regexp_light` gem | regex_light (ASCII, no `\|`, no lazy) | 2465 |
| Moddable XS (JS for ESP32/nRF52) | built in, full ES2018+ | own xsre.c (11 600 lines) | not measured |
| SGScript | built in | sgregex | 5012 |
| ESP-IDF newlib (what you get "for free" on ESP32) | POSIX `regcomp` | Spencer 1994 BSD | 16216 |
| Wren, mJS, Elk, uLisp, Toit, AtomVM, PocketPy, tiny-js, Pawn, AngelScript, Gravity, Lily | none | – | – |
| mruby | gem only (PCRE / Onigmo) | desktop library | several hundred KB |
| TinyGo | Go regexp (does not pass the tests on an MCU) | Go stdlib | – |
| Rust no_std | regex-lite | PikeVM | 94 KB |

Evidence that size is the reason for leaving it out (quotes and URLs in `research/results/survey:languages.json`):

- Lua (PiL 20.1): "POSIX regexp is more than 4000 lines, bigger than all the Lua standard libraries together".
- Wren #120: the author was afraid that regexp "would add a lot of code to Wren".
- Espruino: the `SAVE_ON_FLASH` build defines `ESPR_NO_REGEX` (4 boards out of 71: HYSTM32_28, MICROBIT1, NRF51822DK, STM32VLDISCOVERY; the check refuted the survey's "15 boards" claim), and the "regex optimisations" can also be turned off "to free up flash".
- CircuitPython: `CIRCUITPY_RE` is disabled on every SAMD21, because it "does not fit in 256 kB of flash".
- MicroPython: `MICROPY_PY_RE` only from the `EXTRA_FEATURES` ROM level upwards.
- V7 (Cesanta): the minimal profile drops RegExp first.
- AtomVM (2026-03 issue): tiny-regex-c "looks perfect for an MCU", but it was rejected because it "does not do UTF-8, and we use UTF-8".

Conclusion: the "regexp is big" perception is real and still shapes decisions today, but the MCU engines actually shipped are 1.5-3 KB and make up 3-11% of the full JS engines (Duktape 3%, JerryScript 5.5%, mquickjs 11%). On ESP32 the POSIX regex available from libc is 16 KB, it mallocs, and it is leftmost-longest, which is no good for a scripting language. The gap is not size in itself, but that **there is no engine below 2 KB with the full modern basics list and UTF-8**. The most-copied re1.5 is missing exactly `{n,m}`, UTF-8, the `i` flag and `\b`, and it crashes on `(a*)*`.

## 4. Why a new engine is justified

1. The v1 feature list does not exist anywhere below 2 KB (section 2). Patching the closest candidates (re1.5 + `{n,m}` + UTF-8 + `i` + `\b` + empty-loop protection) would, by the survey's estimate, take re1.5 to 1.95-2.1 KB, and would inherit the RAM requirement of the bytecode buffer and the compiler's 1.1 KB.
2. Most of the small engines are not reentrant (tiny-regex-c static buffer), malloc (T-Rex, Spencer, re1, Plan 9), or use setjmp (T-Rex).
3. None of the small engines is differentially tested; the corpus found a real bug in every one of them (re1.5: 19 SIGSEGV; tiny-regex-c: negated classes; T-Rex: empty pattern crash).
4. Licence: the two small engines with the best feature ratio (SLRE, regExpEmbedded) are GPL/LGPL.

## 5. Design space and the chosen approach

> **The numbers in this section are estimates made before the spikes.** The measured values are in
> section 6, and they differ in several places (feature costs, stack per iteration, the architecture
> finally chosen). The section is kept in order to document how the decision was made.

The survey examined five approaches with measured data (`research/results/survey_algorithms.json`):

| Approach | Code size (M4) | RAM | Worst case |
|---|---|---|---|
| (a) direct pattern interpretation, recursive backtracking | **1.9 KB measured**, full v1 | 0 heap, 0 static; ~230 B stack per group iteration | exponential, tamed into an error by the step limit (34 B) |
| (b) bytecode + recursive backtracking (re1.5 style) | 1.56 KB measured without v1, estimated 1.95-2.1 KB for full v1; spike: 2.6 KB | bytecode buffer from the caller; 48 B stack per choice point | exponential; `(a*)*` loops forever without separate protection |
| (c) bytecode + explicit backtrack stack | estimated 2.1-2.4 KB | deterministic, 256-512 B stack, overflow = error | exponential |
| (d) Pike VM (lockstep NFA) | estimated 2.3-2.7 KB | 2 thread lists × program length × capture array: 64 instructions, 10 captures = 5.4 KB | **linear**, no blow-up |
| (e) lazy DFA | > 3 KB | MB-sized cache | cannot do captures or the greedy/lazy distinction |

Why (a):

- The pattern is the program: there is no parser/emitter (in re1.5 the compiler alone is 1102 B, half the budget) and no bytecode buffer in RAM.
- `{n,m}` is a counter in the recursion frame (134 B for the whole of `{n}`, `{n,}`, `{n,m}` including parsing), instead of bytecode expansion.
- Speed is not a concern (short strings), linear running time would cost 400-800 B (d), and its RAM requirement is worse on an MCU.
- The recursion depth grows with the length of the text (about 230 B per group iteration), but for short strings it fits in a 2 KB task stack; a step limit and a depth limit are both cheap.

Feature costs (measured by cutting with #ifdef, M4): `\b\B` 106 B, `i` 70 B, `{n,m}` 134 B, step limit 34 B, UTF-8 144 B. Without all five, 1406 B. So the v1 list does not fit in 1000 B, 1500 B needs about two features dropped, and the full v1 fits in 2000 B with headroom.

ISA multipliers (same C, same flags, measured with the PlatformIO toolchains found on the machine): Xtensa LX6 (ESP32) 1.12x compared to Thumb-2 (median; with `-mlongcalls`, which ESP-IDF always passes, 1.23x), RV32IMC (ESP32-C3/C6) 1.37x. A 1.9 KB Thumb-2 engine is therefore 2.1-2.3 KB on ESP32 and 2.4-2.6 KB on ESP32-C3. The target is below 2000 B on Cortex-M4 and "as small as possible" on ESP32.

Semantic decisions (proposal; this is what the corpus measures):

- Leftmost-first (Perl/JS/Python), not POSIX leftmost-longest.
- Empty iteration in a loop: the Perl/Python rule (one empty iteration is accepted, then it exits; `(a*)*` on "b": group 1 = (0.0)). JS differs here (the group stays undefined); this shows up in 26/2081 cases, and we document it.
- The capture of a repeated group: the last iteration (Perl/Python), not JS-style clearing on every iteration.
- `$`: only at the end of the text (JS); we do not adopt the Perl/Python "also before a trailing `\n`" rule.
- `\d\w\s\b`: ASCII; `.` and classes: code point; `i`: ASCII letters only. Invalid UTF-8: it does not crash, it takes the byte as one code point, it does not validate (the validating decoder is 2.2x, 140 B).

## 6. Spike results

The same protocol is behind every number: `tools/measure.sh`, Cortex-M4/Thumb-2,
`arm-none-eabi-gcc 10.3.1 -Os -ffreestanding`, the engine translation unit only,
`.text` + `.rodata`, without linking. The correctness numbers refer to the `tests/corpus/`
seeds, with a Python 3 `re` + `re.ASCII` oracle.

### 6.1 The three algorithm spikes

| Spike | M4 | M0+ | x86-64 | corpus (seed1) | stack |
|---|---:|---:|---:|---|---|
| (a) direct pattern interpretation | **1941** | 1975 | 3356 | 2094/2095 | recurses per group iteration |
| (b) bytecode + recursive backtracking | 2198 | 2220 | 4188 | 2093/2095 | `re_match` 536 B static frame |
| (d) Pike VM | 2817 | 2921 | 5447 | **2095/2095** | linear, but needs a work buffer |

The (b) spike split up: **executor 856 B, compiler 1364 B**. This measurement decided the
architecture: the compiler is 62% of the cost, so if the compiler can leave the device,
the on-board part is an order of magnitude cheaper.

The (d) Pike VM is the only one that reached 100% on all five corpora (15 475 cases),
and the only one with no exponential worst case — but it is the largest, and for
`a{200}` it asks for a work buffer of 1257 words (about 5 KB). For short strings that is a bad trade.

Fuzzing on all three spikes, ASan+UBSan, 60 s: 31.8 M / ~30 M / 51.7 M cases, **zero crashes**.

### 6.2 The chosen architecture: product A

A separate JavaScript compiler plus a C matcher VM. The VM as measured:

```
cortex-m4  text=546 data=0 bss=0     (re_match 476, u 70)
cortex-m0+ text=538
x86-64     text=908
stack: re_match 64 static, u 12 static — constant
undefined symbols: none
```

Corpus: seed1 1955/1955, seed7 2872/2872. Fuzz: 16.6 M iterations with an exactly sized
workspace, zero overruns.

`-ffreestanding` is not cosmetic: without it gcc generates a `memset` call from the
capture-clearing loop, and the "no libc dependency" claim falls. That is why `measure.sh`
passes it, and why the CI gate lists the undefined symbols.

### 6.3 Product B: compiling on the device vs. no compilation

We measured two forms for the same job — handling patterns that arrive at run time:

| Form | M4 | `i` support |
|---|---:|---|
| B-lite: C compiler (2250) + VM (546) | **2796** | none |
| direct: no compilation step | **1849** | none (`-DRE_NO_ICASE`) |
| direct + ASCII `i` | 1941 | ASCII |

Like for like, the form without compilation is **947 bytes (34%) smaller**, and it
even does ASCII `i`, which B-lite does not do at all.

The reason is structural, not a matter of tuning. In the B-lite compiler the *encoder* for the
compact bytecode is about 570 bytes: merging class ranges into the 7-bit count + negation bit +
varint UTF-8 format (`base_ranges` 128 + `emit_class` 142 + `class_atom` 84 +
`range` 80 = 434), plus the emitter and the backpatching (`insert` 46 + `offset` 32 +
`copy` 28 + `byte` 28 = 134). The direct interpreter never builds this up: it reads the same
class straight out of the pattern text every time.

**The lesson in general**: compact bytecode is an asset when the compiler does not travel
with it, and a burden when it does. In product A the format buys the 546-byte
matcher; in B we would pay for the encoder *and* the decoder.

We ruled out that this is a matter of compiler settings. The B-lite compiler breaks down into
15 functions, so we suspected call overhead — measured, it is **not** that:

```
-Os                                                        2250   fn=15
-Os -finline-functions                                     2250   fn=15
-Os -finline-functions --param max-inline-insns-auto=600   2250   fn=15
-O2                                                        3852   fn=10
-O3                                                        5228   fn=8
```

gcc already decides optimally at `-Os`; with forced inlining it gets 71% bigger.

### 6.4 Shrinking the direct engine

First round, eight parallel directions, each with independent verification:

| direction | M4 | Δ |
|---|---:|---:|
| Lua/subreg-style rewrite | 1774 | −167 |
| Thumb-2 micro-optimisation | 1802 | −139 |
| function merging | 1803 | −138 |
| character classes | 1819 | −122 |
| alternation | 1820 | −121 |
| entry point | 1832 | −109 |
| quantifiers | 1845 | −96 |
| workspace (bounded stack) | 1973 | +32 |

**Combined: 1634 B** (m0+ 1678, x86-64 2868). Separately the eight directions promised 892
bytes, of which 307 materialised.

This is the round's most important lesson, and it was an orchestration mistake: six agents
found the same three merges (`class_scan`→`atom`, `skip_token`→`atom`,
scanner merging), because they were given overlapping areas. **In parallel optimisation the
areas must be handed out disjointly, or a relay must be built**, where every stage starts
from the previous winner's file.

Two negative results that save time in the future:

- Replacing the matcher core with a foreign structure (the goto-based tail recursion of Lua's
  `lstrlib`) brought **no** gain — the existing frame chain is competitive. The −167 bytes
  came from elsewhere.
- About 35 micro-experiments (noinline variants on every helper function, unsigned
  range tests, table-driven escape translation, alternative `decode`/`pat_char`
  signatures, `re_state` field order) were all neutral or worse.

### 6.5 The state of the 1634-byte engine (historical, see 6.8)

Gates: zero dynamic stack frames, empty undefined list, zero warnings on two
toolchains and in 13 feature configurations, both corpora identical to baseline (the only
failure is the intended lack of `é`/`É` Unicode folding), 29 M sanitizer cases without a crash,
72/72 hand-written feature tests alive, 2.4 M fresh differential cases with zero differences.

Feature ledger on the 1634-byte engine (the size reached by turning the feature off):

| turned off | M4 | Δ |
|---|---:|---:|
| `RE_NO_ALT` (groups + alternation) | 1190 | −444 |
| `RE_NO_CLASSES` | 1496 | −138 |
| `RE_NO_BOUNDED` (`{n,m}`) | 1514 | −120 |
| `RE_NO_WORDB` (`\b\B`) | 1538 | −96 |
| `RE_NO_UTF8` | 1560 | −74 |
| `RE_NO_LAZY` | 1562 | −72 |
| `RE_NO_CAPTURES` | 1594 | −40 |
| `RE_NO_BUDGET` | 1598 | −36 |
| `RE_NO_ICASE` | 1604 | −30 |

Combinations below 1400 do exist (`CLASSES`+`BOUNDED` = 1376, `CLASSES`+`WORDB` = 1388,
`BOUNDED`+`WORDB` = 1418), but these are **profiles, not the all-in engine**. With the full
feature set, the 1400 target needs structural savings; it is demonstrably not reachable with
constant and compiler-switch tricks (see 6.3 and 6.4).

### 6.6 What the design got right and what it did not

The estimates in section 5 were made before the spikes. In hindsight:

- **It got right** that (a) direct interpretation is the smallest — only for product B, not
  product A. A was taken to 546 by moving the compiler off the device, which section 5 did
  not take into account.
- **It overestimated** the stack consumption: the estimate was ~230 B per group iteration,
  the measured value is 104 B per iteration (M4) — that is, more favourable, but the fact of
  *linear growth* remained, and this is product B's most important open question (see
  `docs/all-in-engine.md`).
- **Stale** feature costs: instead of section 5's estimate of `\b\B` 106 / `i` 70 / `{n,m}` 134 /
  step limit 34 / UTF-8 144, the measured ledger is in 6.5.

### 6.7 Bytecode backend experiments (product A)

16 working VM variants, each on 7197 test cases, with zero semantic differences, under ASan/UBSan.
The bytecode saving is a value measured over 3587 **distinct** corpus programs (against the 261247-byte
baseline), not weighted by frequency.

| variant | M4 VM (object) | bytecode saving |
|---|---:|---:|
| baseline | 546 | 0% |
| **tail-sharing** | **546** | **8.37%** |
| CHAR + tail | 562 | 16.19% |
| CHAR + escape jumps + tail | 584 | 27.71% |
| + merged repetition | 686 | 30.44% |

**Tail-sharing is free.** The VM does not change by a single byte (546 as an object, 548 linked,
M0+ 568 — byte for byte the baseline), because this is a purely compiler-side post-pass: it decodes
the finished bytecode into a graph, merges the instructions with completely identical continuations
(automaton minimisation with a fixed point), then re-encodes. Alternatives that end the same way
collapse onto a shared tail. Concrete examples: `(a|ab)+` 64→45, `(?:abc|xbc)+` 80→45, `abc|xbc`
44→36. Anything that contains no shareable continuation (`abc`, `\bfoo\b`, `^a{100}$`) stays unchanged.

It has two safety properties that make it risk-free to enable: it only uses the result if it is
**actually smaller**, and if a relocation would not fit in the 16-bit offset, it drops the candidate
instead of the original. So it can never grow a program and never produce an invalid one.

What the measurement does NOT solve: the size of the on-board C compiler. The new compact toolchain is
3446 bytes instead of the original 2796 — that is, this direction moves product A forward and says
nothing about product B's 1400-byte question.

Two further directions, measured but not adopted:

- **CHAR + escape jumps (584 B VM, 27.71%)**: 72380 bytes of aggregate bytecode saving for
  +38 bytes of VM. On the corpus average the break-even point is two distinct patterns, but this is
  decided by the application's pattern set. The escaped jump format is not compatible with the old
  operand encoding, so the compiler and the VM **must be shipped together**.
- **Counted repetition**: `^a{100}$` is 218 bytes even in the chosen format (instead of the
  original 416). A counted-repetition instruction would remove this linear expansion, but first the
  cost of the counters and of capture restoration has to be measured.

### 6.8 Shrinking product B: two rounds, 1712 -> 1518

Two workflows ran, with deliberately orthogonal assignments.

**Algorithmic relay (1712 -> 1536).** Three variants per stage, the best one always goes on,
with disjoint areas — this is the fix for the first round's overlap mistake.

| stage | result | Δ |
|---|---:|---:|
| parse_quant | 1674 | −38 |
| atom (+ pat_char merged in) | 1614 | −60 |
| match + group | 1574 | −40 |
| closing round | 1536 | −38 |

**Size-coding round (1712 -> 1582 on its own).** Seven demoscene techniques, each measured separately
and verified independently: dispatch form and opcode renumbering −74, IT blocks and branch form
−66, hand-written Thumb-2 assembly −46, data overlap −44, decode loop −12, struct layout −8,
pointer→index 0. Naive sum −264; actually merged, **−130**, because the techniques also overlap
each other.

**The two together: 1518.** And here is the essential lesson: **out of the size-coding round's entire
toolbox, 18 bytes were left on top of the relay's result** (three T2 hunks, the decode loop and one
exploratory item). The two rounds found 80% of the same things — the relay's `decode()` already used
the same length trick, its `is_word` already returned a bit set, and it had reshaped the `walk()`
signature the same way.

**The encoding-level headroom is exhausted with this.** The anatomy measured the ceiling: the full Thumb-2
instruction set is worth a net **32 bytes** on this code (1.9%) — the Thumb-1 build was 1744 against
1712, and on four functions the dumber ISA produced *smaller* code. So the global ceiling of the
"get it down to 16 bits" technique is small, not 200+ bytes. The next bytes will come from the
algorithmic or the structural side.

**We did not adopt the hand-written assembly.** It brought −46 bytes with a portable C fallback, but by
the time it was finished it was stale: the merged `atom()` is no longer the function it was written for.
On top of that it would give a two-file product tied to Thumb-2, 254 bytes of which a sanitizer never
runs over, and whose correctness would be kept in check only by a qemu differential that would have to
be added to CI.

### 6.9 What the corpus does not catch

The relay's atom stage found a regression that was **green on both corpora**: moving the
`RE_BAD` sentinel from −1 to 0 was worth 8 bytes, but an overlong UTF-8 sequence,
`F0 80 80 80`, decodes to code point 0, so 0 is a genuine pattern character. Those patterns returned
`RE_ERROR` instead of the literal that never matches — 245 differences out of 13 million cases.

**The corpora do not cover bad UTF-8; only differential fuzzing catches this.** This is a known gap in
the test strategy, and `feature.c` does not close it either. Anyone touching the decoder or the
sentinel values should run differential fuzzing, not just the corpus.

### 6.10 Measurement lessons

These are not about the engine but about how we measure — and they refuted one of our
documented claims.

**Object code size hides what the linker pulls in.** For a long time `measure.sh` reported only the
object's `.text`. Product A's VM is 538 bytes as an object on Cortex-M0+, but because of the Thumb-1
jump table it pulls `__gnu_thumb1_case_uqi` in from libgcc, so linked it is about 560 bytes.
On Cortex-M4 (Thumb-2) this does not happen. The "no libc dependency" claim was therefore true on M4
and not on M0+ — and the CI gate did not notice, because it only ran `nm -u` on the M4 object.
Fixed: `measure.sh` lists the undefined symbols for both ARM targets, CI fails on M4 and warns on M0+.

The two options as measured: accept the libgcc helper and admit the linked 560 bytes,
or compile for M0+ with `-fno-jump-tables` and get 586 bytes with no dependency. Open decision.

Product B (the direct interpreter) is unaffected: its dispatch is an if chain, not a switch, so it is
clean on both targets.

**Two practices worth keeping:**

- Undefined symbols must be checked for EVERY target, not just the primary one.
  A change of ISA brings a different code generation pattern, and with it a different run-time
  dependency.
- The measured state must be pinned with a hash. The bytecode backend research saves the
  SHA-256 of the input files with every result, so a number can later be traced back
  unambiguously to the source it came from.

## 7. Test strategy

Oracles on the machine: `pcre2test` 10.47 (Perl semantics, 2079/2081 agreement with the corpus), Python 3.11 `re` (`re.ASCII`), Node/V8 (2055/2081, the differences are all the JS empty-iteration rule). Python is the primary oracle, because with a single flag it gives exactly the v1 semantics.

1. **Unit tests** (host): table cases in the Rust `regex-test` format (byte spans, `[]` = no group), a 20-line MinUnit-style harness, `-Wall -Wextra -Werror -fsanitize=address,undefined`. Seeds: the hand-written cases of the corpus, Go `regexp/testdata` (the leftmost-first variant of the Fowler .dat, ~350 lines), CPython `re_tests.py` (177 cases in the subset), test262 (112), PCRE2 `testinput1` (499 patterns). Limit cases (pattern/text/group/depth at limit-1, limit, limit+1), invalid patterns (error, not crash), invalid UTF-8.
2. **Conformance corpus** in the repo: `corpus_wb_seed1/7` (2103 + 3103 cases) with expectations, cross-checked with `pcre2` and JS, the differing cases carrying a semantics label. Zero failures is the requirement.
3. **Differential fuzzing**: (a) 1e5 cases nightly with fresh seeds; (b) RE2-style exhaustive enumeration: every pattern of ≤4 atoms over {a,b} × every string of length ≤5 over {a,b,c}, against Python; (c) a libFuzzer/AFL++ harness (input split at the first NUL into pattern|text, `-timeout=2`), with the step limit and the stack bound asserted, in Linux CI; (d) a Hypothesis `from_regex` property test.
4. **Size gate** on every push: `measure.sh` → Cortex-M4 text+data vs. the committed `size_baseline.txt`; it fails above 2048 B or above baseline+16 B, unless the same commit raises the baseline; per-function diff (`nm --size-sort`); the largest `-fstack-usage` frame is gated too.

Gaps in the existing harness (`tools/gen_corpus.py`, `tools/difftest.py`) that have to be filled at the start of the project: upper-case letters in the text pool (the `i` flag is barely exercised), `\n` in the random texts, batch mode (instead of fork-per-case), crash/signal accounting, JSON output.

## 8. How to start

Repo layout (proposal):

```
src/re.c src/re.h          the engine, one translation unit, C99, 0 libc
test/                       unit + corpus + fuzz harness
tools/measure.sh            size measurement (Cortex-M4, M0+, x86-64)
tools/gen_corpus.py         corpus generator (Python re oracle)
tools/difftest.py           differential runner
size_baseline.txt           size gate
SEMANTICS.md                the fixed semantic decisions
research/                   this study, spikes, measurements
```

Steps:

1. **Commit zero**: tools (measure, gen_corpus, difftest), the CI size gate, SEMANTICS.md with the decisions above. Half a day.
2. **Core**: starting from the spike — literal, `.`, `^ $`, classes, `\d\w\s`, `* + ?`, searching, capture. Green corpus on the feature subset, size in the ledger. 1-2 days.
3. **Alternation + groups + `{n,m}` + lazy** the same way, one at a time, size + corpus after every step. 1-2 days.
4. **UTF-8, `i`, `\b`**, the step limit, error codes, invalid patterns. 1 day.
5. **Hardening**: fuzzing (ASan/UBSan, libFuzzer), exhaustive enumeration, stack measurement on Cortex-M4 (`-fstack-usage`), documented limits (max captures, max depth, step limit). 1-2 days.
6. **Size optimisation** only behind green tests, based on the per-function breakdown; target ≤ 1800 B on M4, so that it stays around 2 KB on ESP32 as well.
7. **Integration example**: a 30-line example of how it is bound into a VM (string + flags → span/capture array), plus a size report for nRF52 and ESP32 (the Xtensa toolchain is under PlatformIO).

Working method: size-driven development. Every commit message contains the M4 byte count; the feature ledger (what each feature costs) is part of the README.

## 9. Risks

- **Stack** is not zero even on short strings: ~230 B per group iteration; 200 characters of text with `(a|b)*` can overrun a 2 KB RTOS stack. A depth limit is needed, and the caller supplies it. (The Pike VM would solve this for 400-800 B of code and more RAM; a v2 option.)
- **Catastrophic backtracking** on patterns like `(a+)+$`: the step limit returns an error code instead of hanging; the documentation has to say so.
- **Semantic differences** from JS (empty iteration, `$`): if the host language is JS-like, SEMANTICS.md must record which rule we follow.
- **ESP32 RISC-V** (C3/C6): 1.37x, so the 2 KB budget means 2.4-2.6 KB there.
- **gcc version**: the same source is 1886 B with gcc 10.3 and 2176 B with Apple clang `-Oz` (+13%). The size gate should be tied to a fixed toolchain.

## 10. Open decisions for Boogie

Decided (the decision lines are in `DECISIONS.md`, with date and rationale):

1. ~~Semantics: Perl/Python or JS?~~ — Perl/Python leftmost-first; repetition of a nullable atom
   was dropped from v1, so the empty-iteration rule is not needed either.
2. ~~Licence?~~ — MIT.
3. ~~API: a one-step `re_match` or a separate `compile`?~~ — Both, as two separate products:
   A = external JS compiler + 546-byte VM (pre-compiled pattern), B = direct interpreter with
   no compilation (pattern arriving at run time).
4. ~~Stack bound: a fixed constant or supplied by the caller?~~ — Supplied by the caller. In product A this is
   `re_match(..., const void **workspace, unsigned depth)` plus a separate `RE_SPACE` error code;
   in product B it does not exist yet (see `docs/all-in-engine.md`).

Open:

1. **Size budget on ESP32.** The 2000 B applies to Cortex-M4; with the measured 1.12x on ESP32
   (1.23x with `-mlongcalls`) and the 1.37x multiplier on ESP32-C3, product A's 546 bytes become
   612-672 and 748 bytes respectively. Is that acceptable as it is, or do we need a separate ESP32 target number?
2. **Product B's shipping condition.** It has two open bugs today (unbounded stack on a
   quantified group, `(a*){65536}` crash). Introducing the bounded stack solves
   both, but it costs size. What is the order: 1400 bytes first, then the bounded stack,
   or the other way round?
3. **The contract of `i` in product B.** ASCII-only today. With the host hook layer the embedding
   language can supply a Unicode `other_case` — should that be the default expectation, or
   should it stay an optional extra?
4. **The `tuned` variant.** 10 bytes smaller (1624), but it contains an `RE_ERROR` vs
   `RE_NOMATCH` difference at 9-10 digit `{n,m}` bounds, which neither the corpus nor the
   feature tests catch. Is the 10 bytes worth it, or does strict baseline identity matter more?
