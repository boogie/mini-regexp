# mini-regexp

A tiny regular-expression engine in C99 for embedding into small scripting languages on
microcontrollers (nRF52, ESP32 and similar).

Regular expressions are missing from most embedded scripting languages for one reason: the engines
are too big. Lua ships pattern matching instead of regex and says so in the manual. MicroPython
puts `re` behind a build flag. AtomVM rejected tiny-regex-c for lacking UTF-8. mini-regexp exists
to make that trade unnecessary.

**Status: experimental.** Both engines pass the differential corpus for their supported scope. API
hardening and wider fuzzing remain before the first release. The first consumer is MagiScript,
whose compiler emits the product A bytecode and whose device engine embeds the VM; that bytecode
format is frozen as v1 (see `DECISIONS.md`).

## Results

Two engines are maintained deliberately, for two different situations.

| | Cortex-M4 | Cortex-M0+ | x86-64 | pattern source |
|---|---:|---:|---:|---|
| **A** — bytecode VM | **546 B** | 538 B | 908 B | compiled off-device |
| **B** — all-in interpreter (core) | **1518 B** | 1586 B | 2830 B | supplied at runtime |

Measured with `tools/measure.sh`: `arm-none-eabi-gcc 10.3.1 -Os -ffreestanding`, engine
translation unit only, `.text` including `.rodata`. Both have zero `data` and `bss`.
Product B's optional host Unicode hook layer adds 40 B when compiled out of the API surface and
134 B when enabled; see `docs/all-in-engine.md` for the complete product accounting.

Sizes throughout this repository are for **Arm Cortex-M4** and **Cortex-M0+**, the 32-bit
microcontroller cores in parts like the nRF52 — not Apple Silicon. x86-64 is reported alongside
them because it is what most contributors build on.

Read the two numbers for what they are. **Product A's 546 bytes is a matcher VM, not an engine
that accepts a pattern** — patterns are compiled on a host and the bytecode is additional flash
per pattern, so a device running A cannot take a pattern from a config file or the network.
Product B's 1518-byte core is the number to compare against a conventional engine, because it does
what a conventional engine does.

### Measured against the field

Thirteen other C engines were built from pinned commits and driven through the same 32-row feature
probe over a common CLI contract — literals, anchors, quantifiers, `{n,m}`, lazy, alternation,
leftmost-first, groups, captures, classes, ranges, `\d \w \s`, word boundaries, UTF-8 code points
and `i`. Rows passed, against measured Cortex-M4 size:

| Engine | Cortex-M4 | Probe rows passed |
|---|---:|---:|
| [pike-tpop](https://www.cs.princeton.edu/courses/archive/spr09/cos333/beautiful.html) | 156 | 5 of 32 |
| **mini-regexp A** | **546** | **32 of 32** |
| [kokke/tiny-regex-c](https://github.com/kokke/tiny-regex-c) | 1162 | 14 |
| [Lua `lstrlib` patterns](https://github.com/lua/lua/blob/master/lstrlib.c) | 1481 | 18 |
| **mini-regexp B** | **1518** | **32 of 32** |
| [MicroPython re1.5](https://github.com/pfalcon/re1.5) | 1530 | 21 |
| [subreg](https://github.com/mattbucknall/subreg) | 1704 | 19 |
| [SLRE](https://github.com/cesanta/slre) | 1946 | 20 |
| [rsc-re1](https://github.com/rsc/re1) | 2468 | 12 |
| [Spencer 1986](https://github.com/garyhouston/regexp.old) | 2751 | 15 |
| [plan9 libregexp](https://github.com/9fans/plan9port) | 3137 | 18 |
| [T-Rex](http://tiny-rex.sourceforge.net/) | 3144 | 23 |
| [musl regex](https://git.musl-libc.org/cgit/musl/tree/src/regex) | 13309 | 28 |
| [QuickJS libregexp](https://github.com/bellard/quickjs) | 13334 | 32 |

Counts are after discounting passes the probe showed were produced by something other than the
feature under test — a missing feature re-read as literal text that also happens not to match, for
instance. Every discount is footnoted in `docs/comparison.md`.

**The strongest claim this evidence supports:**

> On this 32-row probe, mini-regexp answers every row exactly as ECMAScript does, in 546 bytes for
> the precompiled-bytecode engine and a 1518-byte core for the all-in engine. The smallest other engine
> measured that answers all 32 rows is QuickJS's libregexp at 13334 bytes — 24× product A and 8.8×
> product B. Of the thirteen other engines measured, exactly one is smaller than product A, and it
> answers 5 rows.

It is **not** the world's smallest regex engine. pike-tpop is 156 bytes, 3.5× smaller, and it is a
real matcher — it just supports literals, `.`, `*` and edge-anchored `^`/`$`. Nor is 32 rows a
conformance suite; the differential corpus is the conformance evidence. The survey catalogued 51
implementations and measured 13, so a smaller feature-equivalent engine could exist among the rest.
`docs/size-claim.md` states the claim and its limits precisely.

### What the probe found out about everyone else

Worth knowing before you pick any small engine, including this one: **the dominant failure mode in
this field is a silently wrong answer, not an error.** Missing features are rarely rejected — they
are re-read as literal text and produce a confident, plausible, wrong result. `a{2,3}` matches the
six-character string `a{2,3}` on seven of the engines measured. `\b` is the backspace byte on two
of them and the letter `b` on two more. For an embedded target that is worse than a compile
failure: a pattern written from Perl or JavaScript habits loads cleanly and quietly matches the
wrong thing.

The probe also corrected three reputations, ours included. Lua's `lstrlib` **does** have word
boundaries through its `%f[set]` frontier patterns. musl's regex is not strict POSIX ERE — it kept
TRE's extensions, so `\d`, `\w`, `\s` and `\b` all work. T-Rex carries more syntax than its size
suggests. Any note claiming otherwise, including earlier notes in this repository, was wrong.

## Which engine

**Use A when your patterns are known at build time.** Patterns compile on your workstation with a
Node.js script, and only the bytecode ships. The device carries 546 bytes of matcher plus the
compiled patterns.

**Use B when patterns arrive at runtime** — a scripting language reading source at runtime, a
configuration reload, a protocol field. It interprets the pattern string directly, so there is no
compile step and nothing to store but the pattern text.

B is not A plus a compiler. We measured that shape: a C compiler emitting A's bytecode costs
2250 bytes, so compiler-plus-VM is 2796 — worse than interpreting the pattern directly. The
compact format earns its keep only when the compiler stays off the device. `docs/all-in-engine.md`
has the full measurement.

## What it is good for

- Short subjects: configuration values, protocol fields, command lines, user input in a REPL.
- Flash-constrained targets where a 3 KB engine is not affordable but 0.5–1.5 KB is.
- Deterministic memory: no `malloc`, no globals, reentrant, and the caller owns every buffer.
- Bounded worst case: a step budget turns catastrophic patterns into an error instead of a hang,
  and C-stack use does not grow with the input.

## What it is not good for

- **Throughput.** This is a backtracking matcher tuned for size, and speed was explicitly traded
  away. Do not scan kilobytes with it, and do not put it in an interrupt handler.
- **Untrusted patterns without a budget.** Backtracking is exponential in the worst case. The step
  budget makes that an error rather than a hang, but you must pass a sensible one.
- **Untrusted bytecode.** Product A's VM trusts its input completely; it is compiler output, not a
  parser. Never feed it bytes from the network.
- **Full Unicode.** `.`, classes and ranges work on code points, but `\d \w \s \b` are ASCII, and
  case folding is single-code-point only — `ß` does not match `ss`.
- **Regex features beyond the basics.** No backreferences, no lookaround, no named groups, no
  POSIX classes. `docs/all-in-engine.md` and `SEMANTICS.md` state the exact scope.

## Quickstart

Compile a pattern to a C header:

```sh
node tools/re_compile.js '^(\w+)=(\d+)$' setting_re
```

That emits the bytecode and a capture-slot count:

```c
enum { setting_re_capture_slots = 6 };
static const unsigned char setting_re[] = {6,7,51,0,10,0,1,10,2,8,3,0, /* ... */ };
/* "^(\\w+)=(\\d+)$" : 60 bytes */
```

Then match. The caller owns both buffers, so nothing is allocated:

```c
#include "re.h"
#include "setting_re.h"

enum { DEPTH = 16 };                       /* pending backtrack choice points */
static const char *captures[setting_re_capture_slots];
static const void *workspace[(setting_re_capture_slots + 2u) * DEPTH];

int r = re_match(setting_re, "timeout=30", captures, workspace, 10000, DEPTH);
if (r == RE_MATCH) {
    /* captures[0..1] is the whole match, [2..3] group 1, [4..5] group 2 */
    printf("key=%.*s value=%.*s\n",
           (int)(captures[3] - captures[2]), captures[2],
           (int)(captures[5] - captures[4]), captures[4]);
}
```

```
timeout=30   key=timeout value=30
retries=5    key=retries value=5
name=abc     no match (0)
```

The complete program is `examples/settings/`. Build and run it with:

```sh
cc -std=c99 -Isrc -Iexamples/settings -o /tmp/settings \
   examples/settings/main.c src/re.c && /tmp/settings
```

`examples/mynewt/` shows the same thing with task-owned static workspace for nRF52.

### Sizing the buffers

For the pattern above, on a 32-bit target:

| | cost |
|---|---:|
| bytecode | 60 B flash |
| captures | 6 slots = 24 B RAM |
| workspace at `DEPTH = 16` | 512 B RAM |

`captures` needs `<name>_capture_slots` pointers. `workspace` needs
`(capture_slots + 2) * depth` pointers; `depth` is the maximum number of pending choice points,
and running out returns `RE_SPACE` rather than a wrong answer, so you can tune it downwards and
watch for that code. Use the emitted `_capture_slots` enum for static sizing — the
`RE_WORKSPACE_SLOTS(code, depth)` macro reads the bytecode array and is not a constant expression.

Return values are `RE_MATCH`, `RE_NOMATCH`, `RE_BUDGET` (step budget exhausted) and
`RE_SPACE` (workspace exhausted).

### Runtime patterns

Product B takes the pattern as a string and needs no compile step:

```c
int r = re_match(pattern, text, flags, caps, ncaps, workspace, sizeof workspace);
```

It is a spike rather than a shipping product; `research/spikes/direct/BUILD.txt` has the API,
the reproduction commands and the open items.

## Feature set (v1)

| Feature | Notes |
|---|---|
| literals, `.`, `^`, `$` | `.` matches any code point except `\n`; no multiline mode in v1 |
| `\d \w \s` and `\D \W \S` | ASCII only |
| `[...]`, `[^...]`, ranges | ranges by code point, so `[á-ű]` works |
| `* + ? {n} {n,} {n,m}` | greedy, and lazy with a trailing `?` |
| `\|`, `(...)`, `(?:...)` | capturing and non-capturing groups, nested |
| `\b`, `\B` | ASCII word boundary |
| `i` flag | Unicode simple case folding, expanded by the JavaScript compiler |

Not in v1: backreferences, lookaround, multiline flag (planned for v1.1), multi-code-point case
folding, named groups, POSIX classes.

Semantics are Perl/Python leftmost-first, not POSIX leftmost-longest. Offsets are byte offsets
into UTF-8 text. `SEMANTICS.md` is the precise statement; unbounded repetition of a nullable atom
such as `(a*)*` is rejected at compile time.

## Pattern size

Product A's flash cost is the VM plus the compiled patterns, and the second term grows with your
application. The compiler shares equivalent tails by default — instructions whose opcode, operands
and whole continuation are identical are merged, so alternations ending the same way collapse.
Measured over 3587 distinct corpus programs this removes 8.37% of pattern flash with the VM
byte-for-byte unchanged. `compileRe(pattern, {tail: false})` opts out.

## Testing

Tests are data, not code. `tests/corpus/*.jsonl` holds generated cases with their expected results
from Python 3 `re` with `re.ASCII` as the oracle, and any engine speaking the small CLI contract in
`tests/README.md` can be checked with `tools/difftest.py`. Alongside that:
a hand-written feature gate that asserts 72 features individually, ASan/UBSan fuzzing, a stack
probe that proves C-stack use is constant in the input length, and a size gate in CI.

```sh
tools/measure.sh src/re.c                                    # size and gates
python3 tools/difftest.py <engine> tests/corpus/corpus_wb_seed1.jsonl
```

`CONTRIBUTING.md` describes the gates a change has to pass and the experiments that already have
measured answers.

## Repository layout

```
src/            product A: the matcher VM (one C file + one header)
tools/          the JavaScript compiler, size measurement, corpus generator and runner
examples/       worked integrations
tests/          language-agnostic corpus and the CLI contract
docs/           feasibility study, size claim, product B design notes
research/       measurements of existing engines, and the spikes
DECISIONS.md    decision log, one line each
```

## Acknowledgements

This project is developed with AI assistance. Claude, Anthropic's assistant, produced the
feasibility research, the measurement and test tooling, the early prototypes and the size
optimisation rounds. ChatGPT contributed engine implementation work, including the size-optimised matcher and
compiler that the shipping product A grew from, as well as review. Design decisions,
review and the released code are the author's responsibility.

## License

MIT, see `LICENSE`.
