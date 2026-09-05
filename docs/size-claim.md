# Size claim

The defensible claim for mini-regexp is **smallest known matcher VM with the documented v1 feature
set**, not smallest program that can recognize any regular-expression subset.

## Measurement definition

- Target: Cortex-M4 / Thumb-2, `arm-none-eabi-gcc 10.3`, `-Os`.
- Counted: matcher translation unit `.text` including `.rodata`.
- Excluded but reported separately: compiled pattern bytecode, JavaScript compiler and host test
  adapters.
- Current result: 546 bytes `.text`, 0 data, 0 BSS, and no undefined runtime symbols **on Cortex-M4**.
- Cortex-M0+ carries a caveat: Thumb-1 emits a switch jump table for the opcode dispatch, so the
  object is 538 bytes but pulls `__gnu_thumb1_case_uqi` from libgcc, linking to about 560 bytes.
  Building with `-fno-jump-tables` gives a dependency-free 586 bytes. Which of the two is the
  published M0+ number is an open decision (`DECISIONS.md`). Object `.text` alone would have
  hidden this, which is why undefined symbols are now reported for both ARM targets.
- Hard gate: 600 bytes.
- Stack is reported separately: 64 static bytes for `re_match`, with a transient 12-byte UTF-8
  decoder frame. Backtracking uses caller-provided workspace rather than the C stack.

Reproduce with:

```
tools/measure.sh src/re.c
```

## Pattern bytecode

Product A's real flash cost is the VM plus the compiled patterns, and the second term is the one
that grows with the application. The compiler applies tail sharing by default: instructions whose
opcode, operands and entire continuation are identical are merged, so alternations ending the same
way collapse onto a shared tail. Measured over 3587 distinct corpus programs, this removes 21855 of
261247 bytes — **8.37% of pattern flash for no VM change at all** (546 bytes before and after) and
no semantic difference across 7197 differential cases.

`compileRe(pattern, {tail: false})` emits the unshared program; CI runs both corpora in both modes,
so the pass cannot silently change what a program matches.

Larger savings are available but need a format change and a bigger VM: CHAR plus escaped branches
reaches 27.71% for a 584-byte VM, which is measured but not adopted, since compiler and VM would
have to ship together. See `docs/feasibility.md` section 6.7.

## Product B: the all-in engine

Product B has no compile step and is measured under the same protocol, but it is a different
claim and must not be conflated with the 546-byte figure. It is an experimental spike: current
measurement, target, known defects and the hook-layer accounting are in `docs/all-in-engine.md`.

| build | Cortex-M4 |
|---|---:|
| product A matcher VM (shipping) | 546 |
| product B engine, hooks compiled out | 1518 + 40 |
| product B engine, `-DRE_HOOKS` | 1518 + 134 |

Quoting a product B number without the hook layer understates the product by 40–134 bytes.

## Comparison scope

The feasibility survey catalogued 51 C engines. Thirteen were built from pinned commits and driven
through a 32-row feature probe over a common CLI contract; the full matrix, the per-engine notes
and the reproduction commands are in `docs/comparison.md`.

Both mini-regexp engines answer all 32 rows as ECMAScript does. The smallest other engine measured
that also answers all 32 is QuickJS's libregexp at 13334 bytes. Exactly one measured engine is
smaller than product A: pike-tpop at 156 bytes, which answers 5 rows.

**The claim, stated to survive a hostile reader:**

> On this probe, mini-regexp answers every row exactly as ECMAScript does, in 546 bytes of
> Cortex-M4 `.text` for the precompiled-bytecode engine and 1518 bytes for the all-in engine. The
> smallest other engine measured that answers all 32 rows is 24 times the size of the former and
> 8.8 times the size of the latter.

What it does not claim:

1. **Not "the smallest regex engine".** pike-tpop is smaller and is in the table.
2. **Not a like-for-like comparison at 546 bytes.** That figure is a VM, not an engine that accepts
   a pattern: patterns are compiled on a host, the bytecode is additional flash per pattern, and a
   device running product A cannot take a pattern from a user, a config file or the network. Every
   other engine in the table can. Product B's 1518 bytes is the figure to quote whenever the
   comparison is with a runtime-pattern engine; mixing A's size with B's use case would be
   dishonest.
3. **Not conformance.** Thirty-two rows is a feature probe. The differential corpus — 2103 and 3103
   cases against a Python reference — is the conformance evidence.
4. **Not exhaustive.** Thirteen of 51 candidates were measured. A smaller feature-equivalent engine
   could exist among the rest, and nothing rules out an unpublished or unindexed implementation.
   The honest form is always "of the engines measured here", and the build and probe commands are
   in the repository so the table can be extended or contradicted.

An earlier version of this section carried prose notes about what each engine omitted. Running the
engines disproved several of them — Lua's `lstrlib` does have word boundaries via `%f[set]`, and
musl's regex kept TRE's `\d`/`\w`/`\s`/`\b` extensions. Those notes have been replaced by
measurement.
