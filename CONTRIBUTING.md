# Contributing

Thanks for looking. This project has an unusual constraint — every byte of code size is a design
decision — so the contribution process is built around measurement rather than review opinion.

## The one rule

**No size claim without a measurement.** Not an estimate, not "this should be smaller", not a
reading of the generated assembly. Run the tool and paste the output. Several plausible ideas in
this project's history turned out to make the engine *larger*, and the only reason we know is that
they were measured.

## What you need

- `arm-none-eabi-gcc` (the pinned toolchain is 10.3.1; other versions produce different numbers,
  so say which you used)
- a host C compiler, `clang` or `gcc`
- Python 3 and Node.js, for the test tooling and the pattern compiler

No build system: everything is a direct compiler invocation, and the commands below are the whole
workflow.

## The gates

A change is acceptable when all of these pass. CI runs them, but run them locally first — the
corpus takes about eight seconds.

**Size and structural gates.**

```sh
tools/measure.sh src/re.c                                                  # product A
tools/measure.sh research/spikes/direct/re.c -Iresearch/spikes/direct      # product B
```

This prints `.text` for Cortex-M4, Cortex-M0+ and x86-64, the per-function breakdown,
`-fstack-usage` frames, and undefined symbols for both ARM targets. Three of those are pass/fail:

- product A must stay at or below **600 bytes** on Cortex-M4
- no stack frame may be reported as `dynamic` — a variable-length array is a regression
- the undefined-symbol list must be empty on **both** ARM targets

That last one has caught a real bug: Thumb-1 emits a switch jump table that pulls
`__gnu_thumb1_case_uqi` from libgcc, which the Cortex-M4 build does not, so checking only the
primary target hid a dependency for a while.

**Correctness.** Differential against Python 3 `re` with `re.ASCII` as the oracle:

```sh
cc -std=c99 -Wall -Wextra -Werror -O2 -o /tmp/adapter \
   research/spikes/compiler-vm/corpus_adapter.c src/re.c
TINYRE_VM_ADAPTER=/tmp/adapter python3 tools/difftest.py --batch \
   research/spikes/compiler-vm/corpus_adapter.js tests/corpus/corpus_wb_seed1.jsonl
```

Both corpora must be green. On product B the single permitted failure is
`flags='i' pat='é' text='É'`, which is the deliberate ASCII-only case-folding gap.

**Features.** The corpus is generated, so it can miss a feature entirely. `feature.c` asserts
72 features individually, each with a hardcoded expected return, span and group spans:

```sh
cc -std=c99 -O2 -Iresearch/spikes/direct \
   -o /tmp/feature research/spikes/direct/feature.c research/spikes/direct/re.c && /tmp/feature
# feature rows=72 ok=72 fail=0
```

**Warnings.** Clean under `-Wall -Wextra -pedantic -Wvla` on both the host compiler and
`arm-none-eabi-gcc`, in every feature configuration.

**Memory safety.** ASan and UBSan, at least 60 seconds:

```sh
cc -std=c99 -Wall -Wextra -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all \
   -Iresearch/spikes/direct -o /tmp/fz \
   research/spikes/direct/fuzz.c research/spikes/direct/re.c && /tmp/fz 60 987654321
```

## Two traps worth knowing about

**The corpora do not cover malformed UTF-8.** A change once saved 8 bytes, stayed green on both
corpora, and broke 245 of 13 million differential-fuzz cases: an overlong `F0 80 80 80` decodes to
code point 0, so 0 is a real pattern member and cannot be used as a sentinel. If you touch
`decode()` or any sentinel value, run a differential fuzz against the previous engine, not just
the corpus.

**Product B's stack must stay constant in the input length.** `stackprobe.c` measures peak stack
for `a*`, `.*`, `(a)*`, `(a|b)*` and `(\w+ )*` at 8, 32, 128 and 256 characters; all five must be
flat. Quantified *groups* used to recurse once per iteration, which overflowed a 4 KB task stack
at 34 characters. `research/spikes/direct/BUILD.txt` has the harness commands.

## Please do not re-run these

Each of these has a measured answer already, recorded with its numbers in `DECISIONS.md` and
`docs/feasibility.md`. Re-deriving them costs hours and finds the same result.

- **Compiler choice.** gcc `-Os` beats clang at every optimisation level on both engines
  (A: 546 against 578 at `-Oz`; B: 1518 against 2044).
- **Forced inlining.** `-O2` and `-O3` are far larger than `-Os`; inline-parameter tuning changed
  nothing.
- **Adding a compile step to product B.** Pure re-scanning in the direct interpreter is 62 bytes;
  every compiler-plus-format combination measured worse than having no compiler at all
  (1634 < 1784 < 1896 < 2796). See `docs/all-in-engine.md`.
- **Instruction-encoding work.** The whole Thumb-2 instruction set is worth a net 32 bytes on this
  code, and four functions are *smaller* compiled for Thumb-1. Two independent optimisation rounds
  converged on roughly the same transformations.
- **Struct field reordering**, on any of the structs: 0 bytes. All 34 `re_state` accesses already
  sit inside the 16-bit encoding window.
- **Pointer-to-index narrowing** in the workspace records: 19 variants, none won. Growing the
  record from 12 to 16 bytes costs exactly 0 bytes, so record size is flash-neutral here.
- **Bitmap character classes**: more expensive than the comparisons they replace, and they would
  introduce `.rodata` where there is none.
- **`__builtin_clz` in the decoder**: 4 bytes smaller on Cortex-M4, but pulls `__clzsi2` from
  libgcc on Cortex-M0+ and fails the undefined-symbol gate.
- **`-ffixed-r8` … `-ffixed-r11`**: all worse. Taking high registers away with a flag produces
  spills, not better allocation.
- **Hand-written assembly**: measured at 46 bytes smaller with a C fallback, and rejected. It made
  the product two-file, Thumb-2-only, and never covered by sanitizers. See `DECISIONS.md`.

If you think one of these deserves another look, that is fine — but bring a measurement that
contradicts the recorded one, and say which toolchain produced it.

## Scope

The v1 feature set is in `README.md` and the exact semantics in `SEMANTICS.md`. Backreferences,
lookaround, named groups and POSIX classes are out of scope: they change the matching model, not
just its size.

Product A's bytecode format is frozen as v1 because MagiScript's compiler emits it. A change to
the format needs a version bump and a migration story, not just a patch.

## Pull requests

Include the measurement output for every gate your change touches, and a line for `DECISIONS.md`
if you settled something that the next person would otherwise re-derive. Keep the explanatory
comments — several of them guard invariants that a well-meaning refactor would silently break, and
the corpus would not catch it.

Commit messages describe what changed and why, in English, with the numbers inline.

## Development notes

`CLAUDE.md` holds project conventions and shell hygiene that apply to any automated tooling working
in this repository. It is worth reading even if you are not using such tooling: the traps it lists
are real ones this project hit.
