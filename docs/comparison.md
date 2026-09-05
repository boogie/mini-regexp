# Comparison with other C regex engines

Everything in this document was produced by running the engines, not by reading them. An earlier
version of this comparison carried prose notes about what each engine omitted; those notes were
never verified by execution, and several of them turned out to be wrong. This replaces them.

Reproduce it: each engine builds from the pinned commit in its `research/engines/<name>/BUILD.txt`,
and the probe rows are listed below. The workflow that produced it is recorded in the repository
history.

## The measurement

Everything below was produced by running the engines, not by reading them. Each engine was built
from the pinned commit recorded in its `research/engines/<name>/BUILD.txt`, driven through a common
CLI contract (`./driver [-i] PATTERN TEXT` → `match START END` / `nomatch` / `error`, plus one line
per capture group), and given the same 32-row feature probe. Byte offsets are UTF-8 byte offsets.
Sizes are Cortex-M4 `.text` measured under one protocol (`tools/measure.sh`, `arm-none-eabi-gcc
10.3.1 -Os -ffreestanding`).

### Legend

| Mark | Meaning |
|---|---|
| ● | full — every probe row in the group passed on the engine's own merits |
| ◐ | partial — some rows passed, some did not; see the note |
| ○ | absent, **silently** — the engine accepted the pattern and returned a wrong answer with no diagnostic |
| ⊘ | absent, **cleanly** — the engine rejected the pattern through its own error path |
| – | n/a — the engine's API cannot express the question (usually: no case-insensitive flag exists) |

The distinction between ○ and ⊘ is the single most useful column in this table and is discussed in
"Findings" below.

### Matrix, part 1 — core syntax

Rows sorted by measured size, ascending.

| Engine | Cortex-M4 `.text` | Literals & anchors | `*` `+` `?` | `{n,m}` | Lazy | Alternation |
|---|---:|:---:|:---:|:---:|:---:|:---:|
| pike-tpop | 156 | ◐ ᴬ | ◐ ᴮ | ○ | ○ | ○ |
| **mini-regexp A** | **546** | ● | ● | ● | ● | ● |
| tiny-regex-c | 1162 | ◐ ᶜ | ● | ○ | ○ | ○ |
| lua-lstrlib | 1481 | ◐ ᶜ | ● | ⊘ | ● | ⊘ |
| **mini-regexp B** | **1518** | ● | ● | ● | ● | ● |
| re1.5 | 1530 | ◐ ᶜ | ● | ⊘ | ● | ● |
| subreg | 1704 | ◐ ᴰ | ● | ○ | ○ | ● |
| slre | 1946 | ◐ ᶜ | ● | ○ | ● | ● |
| rsc-re1 | 2468 | ◐ ᴱ | ● | ○ | ● | ● |
| spencer-1986 | 2751 | ◐ ᶜ | ● | ○ | ⊘ | ● |
| plan9-libregexp | 3137 | ● | ● | ○ | ○ | ◐ ᶠ |
| t-rex | 3144 | ◐ ᶜ | ● | ● | ○ | ● |
| musl-regex | 13309 | ◐ ᴳ | ● | ● | ○ ᴴ | ◐ ᶠ |
| quickjs-libregexp | 13334 | ● | ● | ● | ● | ● |

### Matrix, part 2 — structure, classes and text model

| Engine | Groups | Captures | Classes & ranges | `\d \w \s` | `\b \B` | UTF-8 code points | `-i` | Probe rows passed (raw / genuine) |
|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|---:|
| pike-tpop | ○ | ○ | ○ | ○ | ○ | ○ | ○ ᴵ | 9 / 5 |
| **mini-regexp A** | ● | ● | ● | ● | ● | ● | ● | **32 / 32** |
| tiny-regex-c | ○ | ○ | ● | ● | ○ | ○ ᴶ | – | 17 / 14 |
| lua-lstrlib | ⊘ | ◐ ᴷ | ● | ● | ◐ ᴸ | ○ ᴶ | – ᴵ | 19 / 16 ᴸ |
| **mini-regexp B** | ● | ● | ● | ● | ● | ● | ● ᴹ | **32 / 32** |
| re1.5 | ● | ● | ● | ● | ⊘ | ○ ᴶ | – | 22 / 21 |
| subreg | ● | ◐ ᴺ | ● | ● | ○ ᴼ | ○ | ● | 21 / 19 |
| slre | ◐ ᴾ | ● | ● | ◐ ᵠ | ○ ᴼ | ○ ᴶ | ● | 23 / 20 ᴿ |
| rsc-re1 | ● | ● | ○ | ○ | ○ | ○ | ⊘ | 16 / 12 ᴿ |
| spencer-1986 | ◐ ᴾ | ● | ● | ○ | ○ | ○ ᴶ | – | 18 / 15 |
| plan9-libregexp | ◐ ᴾ | ● | ● | ○ | ○ | ● | – | 20 / 18 |
| t-rex | ● | ◐ ˢ | ● | ● | ◐ ᵀ | ⊘ ᵁ | – | 24 / 23 |
| musl-regex | ◐ ᴾ | ● | ● | ● | ● | ● ⱽ | ● | 28 / 28 |
| quickjs-libregexp | ● | ● | ● | ● | ● | ● ᵂ | ● | 32 / 32 |

"Raw" is the number of the 32 rows whose output matched the expectation exactly. "Genuine" removes
passes that the probe showed were produced by something other than the feature under test — a
degraded literal reading that happened to also not match, an adapter shim, or a byte range that
coincidentally covered the subject. Every discount is footnoted; none is a judgement call made from
reading source.

### Notes

- **ᴬ** `.` matches a newline (`a.c` matches `a\nc`), so `dot_nl` is a wrong answer, not a gap.
- **ᴮ** Only `*` exists. `plus` passes because `ab+c` degrades to a literal that also fails to match.
- **ᶜ** `.` matches a newline. All of tiny-regex-c, lua-lstrlib, re1.5, slre, spencer-1986 and t-rex
  fail `dot_nl` for this reason. tiny-regex-c's is a build-time switch (`RE_DOT_MATCHES_NEWLINE`
  defaults to 1); the others are unconditional.
- **ᴰ** `$` is a real metacharacter but `^` is not — `a^b` matches the literal text `a^b`. The
  `anchor_bol` pass comes from the adapter, not the engine.
- **ᴱ** rsc-re1 has no anchors at all. `^abc` matches the literal text `^abc`; both anchor rows pass
  only because the literal reading also fails. Backslash is not an escape character either, so there
  is no way to match a literal `*`, `+`, `?`, `(`, `)`, `|` or `.`.
- **ᶠ** Leftmost-**longest**, not leftmost-first: `a|ab` against `ab` returns the 2-byte match. This
  is a semantic difference from ECMAScript and PCRE, not a missing feature.
- **ᴳ** `dot_nl` passes when the caller sets `REG_NEWLINE`; both builds were run and both recorded.
  The ◐ reflects the default flag set used for every engine here.
- **ᴴ** musl accepts `a.+?c`, `a*?`, `a{2,3}?` without error and silently returns the greedy span.
  A pattern ported from PCRE or JS will not fail loudly; it will quietly match more than intended.
- **ᴵ** The engine has no case-insensitive mode; the `icase` pass is the adapter ASCII-lowercasing
  both sides. Reported as not-supported here so the table does not credit a feature that is absent.
- **ᴶ** `utf8_range` passes as a raw **byte** range that happens to cover the subject's bytes. On
  tiny-regex-c the same class matches a lone `0xC3` lead byte; on lua-lstrlib it matches byte 2 of
  `日`; on re1.5 it matches the invalid pair `B5 B6`. This is not UTF-8 range support.
- **ᴷ** Numbered captures with correct spans work; `(a)(x)?b` cannot be written at all, because Lua
  quantifiers bind to a single character class rather than to a group.
- **ᴸ** The `\b` rows read "unsupported" only because the adapter's Perl→Lua translator has no
  mapping for them. Lua 5.4's frontier pattern gives the exact expected answers:
  `%f[%w]cat%f[%W]` → `match 2 5` on `a cat b` and `nomatch` on `scatter`. `\B` genuinely has no
  equivalent. Counting the frontier form, lstrlib's genuine total is 18, not 16. **Any claim that
  Lua patterns cannot do word boundaries is wrong.**
- **ᴹ** Product B's `-i` is ASCII-only. Both probe rows pass, but beyond the probe
  `-i 'é'` against `É` returns `nomatch` where ECMAScript, musl and product A all match.
- **ᴺ** Captures are appended in completion order, not by position: `(x)?(a)` against `a` reports the
  `(a)` group as slot 1, and `(ab)+` over two iterations reports two separate captures.
- **ᴼ** `\b` is the ASCII backspace byte (0x08), not an assertion. `wordb_neg` is a vacuous pass.
- **ᴾ** `(?:...)` is rejected (lua-lstrlib, spencer-1986, plan9-libregexp, musl-regex) or silently
  mis-parsed (slre). Plain `(...)` works in all five.
- **ᵠ** `\d` and `\s` work; `\w`, `\W` and `\D` return `SLRE_INVALID_METACHARACTER`.
- **ᴿ** The slre and rsc-re1 agents' prose tallies disagreed by one row with their own per-row lists.
  The per-row lists are the primary record and are what this table uses.
- **ˢ** Capture spans are correct on the probe's shapes only. T-Rex does not backtrack, and the
  damage is visible: `(a*)(b)` against `aab` loses group 2 entirely, and `(ab)+` against `ababab`
  reports group 1 as 0–2 instead of 4–6.
- **ᵀ** `\B` is correct; `\b` is present but wrong — `\bcat\b` matches the bare subject `cat` but not
  `cat` inside `a cat b`. Marking `\b` as supported for T-Rex would be materially misleading.
- **ᵁ** Any non-ASCII byte in the pattern is a compile error ("letter expected"). `.` is one byte.
- **ⱽ** This is a host build against a Unicode-aware libc. The UTF-8 and folding rows are a statement
  about musl's regex code *plus* a Unicode-aware `mbtowc`/`iswctype`, not about musl regex on a
  bare-metal target with a stub ctype.
- **ᵂ** libregexp never sees UTF-8 bytes; it matches Latin-1 or UTF-16 and the adapter transcodes and
  maps offsets back. It prints the same line as a natively UTF-8 engine, but the two are not
  equivalent on an MCU.

### Verdict on the size claim

**No. mini-regexp cannot call itself "the world's smallest regex engine", and should not try.**

*Is anything smaller than product A's 546 bytes?* Yes, one thing: **pike-tpop at 156 bytes**, the
~35-line Kernighan and Pike matcher from *The Practice of Programming* §9.2. It is 3.5× smaller and
it is a real, working matcher. What it supports is literal bytes, `.`, `*`, and `^`/`$` recognised
only at the very start and end of the pattern — 5 of the 32 probe rows on its own merits. It has no
error path whatsoever, so `[`, `(`, `+`, `{` and a trailing backslash are all consumed as literal
text. Anyone claiming a smallest-in-the-world title has to get past it, and on size alone nobody in
this survey does.

*What is the smallest engine that passes every row product A passes?* Product A passes all 32. The
smallest **third-party** engine that also passes all 32 is **QuickJS's libregexp at 13334 bytes** —
24.4× larger. The runner-up is musl's regex at 13309 bytes with 28 of 32, and after that the field
drops to t-rex at 3144 bytes with 23. Nothing between 546 and 13309 bytes comes close.

**The strongest claim the evidence supports**, stated so it survives a hostile reader who re-runs the
probe:

> On this 32-row feature probe, mini-regexp answers every row exactly as ECMAScript does, in 546
> bytes of Cortex-M4 `.text` for the precompiled-bytecode engine and 1518 bytes for the all-in
> engine. The smallest other engine measured that answers all 32 rows is QuickJS's libregexp at
> 13334 bytes — 24× the size of product A and 8.8× the size of product B. Of the thirteen other
> engines measured, exactly one is smaller than product A, and it answers 5 of the 32 rows.

Two narrower claims are also clean:

> Product B, at 1518 bytes, is the smallest engine measured here that accepts a pattern **string at
> runtime** and answers all 32 rows correctly.
>
> Every engine measured between 156 and 13309 bytes fails at least four probe rows.

**What this explicitly does not claim.**

1. Not "the smallest regex engine". pike-tpop is smaller and it is in this table.
2. Not that product A is comparable like-for-like with the rest of the field. **546 bytes is a VM,
   not an engine that takes a pattern.** Patterns are compiled on a host by
   `tools/re_compile.js`; the bytecode is additional flash per pattern, and a device running product
   A cannot accept a pattern from a user, a config file, or the network. Every other engine in this
   table can. Product B (1518 bytes) is the number to quote whenever the comparison is with a
   runtime-pattern engine, and any headline that mixes A's size with B's use case is dishonest.
3. Not conformance. Thirty-two rows is a feature probe, not a test suite. mini-regexp's own
   differential corpus (2103 and 3103 cases against a JS reference) is the conformance evidence; this
   table is not.
4. Not exhaustive. The survey catalogued 51 candidate implementations; 12 were built and probed. The
   other 39 were excluded on documented grounds but were not measured, so a smaller
   feature-equivalent engine could exist among them. Nothing here rules out an unpublished, unindexed
   or newer implementation either. The honest form of the claim is always "of the engines measured
   here", and the probe and build commands are in the repository so that anyone can extend it.

### Findings that cut against reputation — including ours

**The dominant failure mode in this field is a silently wrong answer, not an error.** This was the
single most consistent measured result across twelve engines, and it is not what the prose survey
assumed. Missing features are almost never rejected; they are re-read as literal text and produce a
confident, plausible, wrong answer. `a{2,3}` matches the six-character string `a{2,3}` on
pike-tpop, tiny-regex-c, subreg, slre, spencer-1986, plan9-libregexp and rsc-re1. `\b` is the
backspace byte on subreg and slre and the letter `b` on spencer-1986 and plan9. On rsc-re1, `[bcd]`
matches the literal text `[bcd]`. Of the ○/⊘ cells in the matrix above, ○ outnumbers ⊘ roughly three
to one. For an embedded target that is worse than a compile failure: a pattern written in Perl or JS
habits loads cleanly and quietly matches the wrong thing. It is also why the "genuine" column exists
— pasting raw pass counts into a table would credit seven engines with bounded repetition, word
boundaries or UTF-8 ranges that they demonstrably lack.

**Three engines are better than their reputation.** Lua's lstrlib *does* have word boundaries, via
the `%f[set]` frontier pattern, and gives the exactly correct answers for both rows — the adapter
simply never mapped `\b` onto it. musl's regex is *not* strict POSIX ERE: it kept TRE's extensions, so
`\d`, `\D`, `\w`, `\s`, `\b` and `\B` all work. T-Rex supports full `{n}`/`{n,}`/`{n,m}` bounded
repeats, real `(?:...)`, the complete `\d \D \w \W \s \S` set and leftmost-first alternation — more
surface syntax than its 3144 bytes suggests. Any note claiming otherwise for these three is wrong.

**Two engines are worse.** T-Rex's `\b` compiles, runs, and returns wrong answers, and its lack of
backtracking leaks into ordinary patterns: `(a|ab)c` against `abc` returns `nomatch`. re1's
`recursiveloop` executor segfaults on every non-matching input, on 15 of 32 probe rows — an
unguarded `case Any:` walking off the end of the subject.

**Two engines can be made to hang.** subreg does not terminate on `(?:a*)*` against `b`;
spencer-1986 does not terminate on `^(a+)+$` against 30 `a`s and a `!`. Neither was triggered by a
probe row, and both are live denial-of-service risks on a microcontroller.

#### Where mini-regexp loses

These were measured after the probe, against QuickJS libregexp as the ECMAScript oracle, and they
belong in the README.

1. **Product A silently accepts patterns ECMAScript rejects** — the exact hazard criticised above.
   `a)` compiles and matches the literal text `a)`; `a**` compiles and never matches; `(?P<n>a)`
   compiles as a capturing group over the literal text `?P<n>a`. Product B rejects all three
   (`error -1`). On input validation, A is the weaker of our two engines and is no better than the
   field.
2. **Product A rejects unbounded repeats of a nullable atom.** `(a*)*`, `(?:a*)*`, `(a?)*` and
   `(|a)*` all fail to compile ("unbounded repeat of nullable atom"). ECMAScript accepts all four.
   This is a deliberate, documented restriction, and it is cleanly diagnosed rather than mis-matched
   — but it is a real gap that QuickJS does not have.
3. **Product B accepts those patterns and gets two of them wrong.** `(|a)*` against `aa` returns
   `match 0 0` where ECMAScript returns `match 0 2`, and `(a*)*` against `b` reports group 1 as
   `0 0` where ECMAScript reports `-1 -1`. Neither product is ECMAScript-exact on nullable unbounded
   repeats; they are wrong in opposite directions.
4. **Product B has ASCII-only case folding.** `-i 'é'` against `É` and `-i '[à-ÿ]+'` against `ÉÀ`
   both return `nomatch`. musl, QuickJS and product A all match. This is the one deliberate failure
   in B's own differential corpus and it is a genuine loss to two engines in this table.
5. **pike-tpop is 3.5× smaller than product A** and it works. It does very little, but the
   smallest-in-the-world title is not available and the README should say so before someone else
   does.

#### Where mini-regexp wins, beyond the row counts

Both products refuse rather than hang. On `^(a+)+$` against 30 `a`s and a `!`, product A returns a
step-budget error in 0.05 s and product B returns `RE_LIMIT` (`-2`) in 0.23 s. **QuickJS libregexp
did not terminate within 20 s on the same input**, and spencer-1986 did not terminate within 10 s.
A bounded refusal is the correct behaviour for a microcontroller and it is the reason for the
"genuine" gap in rows 2 and 3 above: mini-regexp trades a small amount of ECMAScript completeness for
a hard ceiling on work done. That trade should be stated as a trade, not sold as pure upside.

Verification for our own rows: both products were built exactly as their `BUILD.txt` files record,
sanity-checked before probing (`match 2 5` on the literal row, `nomatch` on `zzz`, correct group
spans on `(a)(b)`, and a reachable error path on `(`), and both returned 32/32. The one row whose
literal output differed from the probe's expectation is `group`: `(ab)+` against `abab` prints
`match 0 4` followed by `1 2 4`. That extra line is the correct ECMAScript group span, and the
QuickJS oracle prints byte-identical output for the same input — so it is scored as a pass under the
same rule the QuickJS run used, not as a deviation.
