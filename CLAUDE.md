# mini-regexp

A code-size-optimised C regex engine for microcontrollers (nRF52, ESP32). Two products are
maintained deliberately: **A** is a JavaScript compiler plus a 546-byte bytecode VM (`src/`),
**B** is an all-in direct interpreter with no compile step (`research/spikes/direct/`).
Read `DECISIONS.md` first — it records what has already been settled and why.

## Shell hygiene

These are not style preferences. Each one has cost real time in this project.

**Never `rm -rf` a path built from variables.** It trips the safety guard and blocks on a
permission prompt, stalling long background runs. Write into a fresh uniquely-named directory
instead of clearing one:

```sh
work=$SCRATCH/try-$(date +%s)-$$      # fresh, no removal needed
mkdir -p "$work"
```

If you genuinely must remove something, use a literal path, or guard it:

```sh
[ -n "$dir" ] && [ -d "$dir" ] && rm -rf "$dir"
```

Do not look for a permissions setting to silence this. It is a critical-path safety check,
and it deliberately overrides `permissions.allow` rules, `PreToolUse` hooks that return
`allow`, and even `bypassPermissions` mode. A variable sitting directly before a path
segment is treated as a root-level removal, because that is what the command becomes when
the variable is empty. Changing the shell is the only fix.

**The interactive shell is zsh, which does NOT word-split unquoted variables.** A flag list in
a variable arrives as one argument and the compile silently fails, leaving a stale object file
that you then measure. This has produced wrong measurements more than once. Either put the
flags in a `.sh` file and run it with `sh`, or use an array:

```sh
# wrong in zsh: gcc $FLAGS -c x.c        -> one giant argument
sh script.sh                             # right: sh does split
flags=(-Os -std=gnu99); gcc "${flags[@]}" -c x.c   # right: array
```

**Do not parse `ls` output in a loop.** `ls` is rewritten to long format here, so
`for f in $(ls -t *.x)` iterates over permission bits and dates. Use a glob, or `find`.

**Always quote paths.** The scratchpad path contains no spaces today, but the session directory
names are long and change between runs.

## Measurement discipline

Every size claim comes from `tools/measure.sh`, never from an estimate:

```sh
tools/measure.sh src/re.c                        # product A
tools/measure.sh research/spikes/direct/re.c -Iresearch/spikes/direct   # product B
```

It reports `.text` for Cortex-M4, Cortex-M0+ and x86-64, the per-function breakdown,
`-fstack-usage` frames and undefined symbols **for both ARM targets**. Three of those are gates:

- no stack frame may be `dynamic` (a VLA is a regression)
- the undefined-symbol list must be empty on both ARM targets — object `.text` alone hides
  helpers the linker pulls in, which is how a Thumb-1 libgcc dependency stayed invisible
- `-Wall -Wextra -pedantic -Wvla` must be clean on host clang and arm-none-eabi-gcc

Correctness is differential against Python 3 `re` with `re.ASCII`:

```sh
cc -std=c99 -Wall -Wextra -O2 -Idir -o /tmp/drv dir/driver.c dir/re.c
python3 tools/difftest.py /tmp/drv tests/corpus/corpus_wb_seed1.jsonl
```

A run takes about 8 seconds, so run it after **every** change. The only permitted failure on the
all-in engine is `flags='i' pat='é' text='É'` — ASCII-only case folding is a deliberate gap.
The corpus does not catch everything: `research/spikes/direct/feature.c` asserts 72 features
individually and must stay at `ok=72 fail=0`.

**Neither the corpus nor `feature.c` covers malformed UTF-8.** A change that saved 8 bytes once
stayed green on both corpora while breaking 245 of 13 M differential-fuzz cases: an overlong
`F0 80 80 80` decodes to code point 0, so 0 is a real pattern member and cannot be a sentinel.
If you touch `decode()` or any sentinel value, run differential fuzzing against the previous
engine, not just the corpus.

## Do not re-run these

Measured, closed, recorded in `DECISIONS.md` and `docs/`:

- **Compiler choice.** gcc `-Os` beats clang at every level on both engines
  (A: 546 vs 578 at `-Oz`; B: 1712 vs 2044). Only gcc 10.3.1 is installed.
- **Forced inlining hurts.** `-O2` and `-O3` are far larger than `-Os`; inline-parameter
  tuning changed nothing.
- **A compile step for product B loses.** Pure re-scanning is only 62 bytes; every
  compiler-plus-format combination measures worse than having no compiler at all.
- **Feature removal is not an option** for reaching a size target — that produces a profile,
  not the all-in engine.
- **Pointer-to-index narrowing** in the workspace records saved nothing — 19 variants, none won.
  The decisive control: *growing* the record from 12 to 16 bytes cost exactly 0 bytes, so record
  size is flash-neutral here and every cost comes from offset arithmetic.
- **Bitmap character classes** are more expensive than the comparisons they replace
  (`\s` +16, `\w` +28 or +32) and would introduce `.rodata` where there is none.
- **`__builtin_clz` in the UTF-8 decoder** saves 4 bytes on Cortex-M4 but pulls `__clzsi2` from
  libgcc on Cortex-M0+, which fails the undefined-symbol gate.
- **`-ffixed-r8` … `-ffixed-r11`** to force low registers: 1728 / 1742 / 1740 / 1722, all worse.
  Taking high registers away with a flag only produces spills.
- **Struct field reordering** on any of the structs: 0 bytes. All 34 `re_state` accesses already
  sit inside the 16-bit encoding window; the three wide ones are wide because of high registers,
  not offsets.
- **Instruction-encoding work in general is closed.** The entire Thumb-2 instruction set is worth
  a net 32 bytes on this code, and four functions are smaller when compiled for Thumb-1.
- **Hand-written assembly** was measured (−46 B with a C fallback) and rejected; see
  `DECISIONS.md` for why.

## Conventions

- README, code and `docs/*.md` in English; `docs/feasibility.md` is the Hungarian internal study.
- One line per decision in `DECISIONS.md`: date, decision, why.
- Commit messages carry no AI attribution trailers.
