---
title: mini-regexp
description: A regex engine small enough to embed — 546 bytes of Cortex-M4 with captures, alternation and UTF-8.
---

# mini-regexp

**A regular-expression engine small enough to put in a microcontroller scripting language.**
546 bytes of Arm Cortex-M4 for precompiled patterns, 1518 bytes when patterns arrive at runtime.
Captures, alternation, UTF-8 code points, lazy and bounded repetition, word boundaries — in C99,
with no `malloc`, no globals, and no C-stack growth with the input.

[Source on GitHub](https://github.com/boogie/mini-regexp) · MIT licensed

---

## Why

Regular expressions are missing from most embedded scripting languages for one reason: the engines
are too big. Lua ships pattern matching instead of regex and says so in its manual. MicroPython
puts `re` behind a build flag. AtomVM rejected tiny-regex-c for lacking UTF-8. The gap is not that
regex is expensive in principle — it is that nothing below 3 KB combined the features people
actually use.

## The numbers

| | Cortex-M4 | Cortex-M0+ | x86-64 | pattern source |
|---|---:|---:|---:|---|
| **A** — bytecode VM | **546 B** | 538 B | 908 B | compiled off-device |
| **B** — all-in interpreter | **1518 B** | 1586 B | 2830 B | supplied at runtime |

`arm-none-eabi-gcc 10.3.1 -Os -ffreestanding`, engine translation unit only, `.text` including
`.rodata`, zero `data` and `bss`. Sizes are for **Arm** microcontroller cores — not Apple Silicon.

Product A's 546 bytes is a matcher VM, not an engine that accepts a pattern: patterns are compiled
on a host, and a device running A cannot take a pattern from a config file or the network. Product
B's 1518 bytes is the number to compare against a conventional engine.

## Measured against the field

Thirteen other C engines were built from pinned commits and driven through the same 32-row feature
probe over a common CLI contract.

| Engine | Cortex-M4 | Probe rows passed |
|---|---:|---:|
| [pike-tpop](https://www.cs.princeton.edu/courses/archive/spr09/cos333/beautiful.html) | 156 | 5 of 32 |
| **mini-regexp A** | **546** | **32 of 32** |
| [kokke/tiny-regex-c](https://github.com/kokke/tiny-regex-c) | 1162 | 14 |
| [Lua lstrlib patterns](https://github.com/lua/lua/blob/master/lstrlib.c) | 1481 | 18 |
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

**This is not the world's smallest regex engine.** pike-tpop is 156 bytes and 3.5× smaller — it
supports literals, `.`, `*` and edge-anchored `^`/`$`, and answers 5 of the 32 rows. What the
evidence does support:

> On this probe, mini-regexp answers every row exactly as ECMAScript does, in 546 bytes for the
> precompiled-bytecode engine and 1518 for the all-in engine. The smallest other engine measured
> that answers all 32 rows is QuickJS's libregexp at 13334 bytes — 24× product A and 8.8× product B.

[The full matrix, with per-engine notes and reproduction commands →](comparison.html)

### One finding worth carrying away

Whichever small engine you pick, including this one: **the dominant failure mode in this field is a
silently wrong answer, not an error.** Missing features are rarely rejected — they are re-read as
literal text and produce a confident, plausible, wrong result. `a{2,3}` matches the six-character
string `a{2,3}` on seven of the engines measured. `\b` is the backspace byte on two of them and the
letter `b` on two more. For an embedded target that is worse than a compile failure.

## Using it

Compile a pattern on your workstation:

```sh
node tools/re_compile.js '^(\w+)=(\d+)$' setting_re
```

```c
enum { setting_re_capture_slots = 6 };
static const unsigned char setting_re[] = {6,7,51,0,10,0,1,10,2,8,3,0, /* ... */ };
/* "^(\\w+)=(\\d+)$" : 60 bytes */
```

Then match. Both buffers are caller-owned, so nothing is allocated:

```c
#include "re.h"
#include "setting_re.h"

enum { DEPTH = 16 };
static const char *captures[setting_re_capture_slots];
static const void *workspace[(setting_re_capture_slots + 2u) * DEPTH];

int r = re_match(setting_re, "timeout=30", captures, workspace, 10000, DEPTH);
if (r == RE_MATCH) {
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

That pattern costs 60 bytes of flash, 24 bytes of RAM for the captures and 512 bytes of workspace
at `DEPTH = 16` on a 32-bit target. `RE_BUDGET` means the step budget ran out; `RE_SPACE` means
the workspace did. Neither can produce a wrong answer, so both are safe to tune down until the
code appears.

## What it will not do

- **Throughput.** A backtracking matcher tuned for size, with speed explicitly traded away. Do not
  scan kilobytes with it.
- **Untrusted bytecode.** Product A's VM trusts its input completely — it is compiler output, not a
  parser.
- **Full Unicode.** `.`, classes and ranges work on code points, but `\d \w \s \b` are ASCII and
  case folding is single-code-point, so `ß` does not match `ss`.
- **Backreferences, lookaround, named groups, POSIX classes.** Out of scope: they change the
  matching model, not just its size.

## How it is tested

Differential against Python 3 `re` with `re.ASCII` over generated corpora of 2103 and 3103 cases,
on top of a hand-written gate asserting 72 features individually, ASan and UBSan fuzzing, a stack
probe proving C-stack use is constant in the input length, and size and undefined-symbol gates in
CI on both ARM targets.

## Reading further

- [Comparison with other engines](comparison.html) — the measured feature matrix
- [The size claim and its limits](size-claim.html) — what is and is not being asserted
- [Feasibility study](feasibility.html) — the survey of 51 engines and every architectural decision
- [The all-in engine](all-in-engine.html) — product B's design, ledger and open items

---

Developed with AI assistance; see the acknowledgements in the
[README](https://github.com/boogie/mini-regexp#acknowledgements).
