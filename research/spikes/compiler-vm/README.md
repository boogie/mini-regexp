# Compiler + tiny VM spike

This spike explores a host-side JavaScript compiler and a minimal iterative backtracking VM in C.
The compiler emits compact relative-jump bytecode; the VM interprets it without libc, heap
allocation or static mutable state.

`tools/re_compile.js` is the dependency-free Node.js compiler and `src/re.c` is the
size-optimized VM promoted from this spike. `corpus_adapter.js` and `corpus_adapter.c` are
host-only test glue for running them through `tools/difftest.py`.

## Current result

- The VM is 546 bytes of Cortex-M4 `.text`, with no data or BSS and a hard 600-byte ceiling.
- Pattern bytecode is additional flash; `\bfoo\b` with the search wrapper is 28 bytes.
- Literals, dot, anchors, classes, alternation, groups, greedy/lazy quantifiers, bounded repeats,
  UTF-8 code points, `\b`, and `i`/`m`/`s` compilation are present. The `i` flag expands Unicode
  simple case-fold equivalents at compile time without adding VM instructions.
- Unbounded repetition of a nullable atom is rejected by the compiler.
- The VM uses constant C stack and distinguishes step-budget exhaustion (`RE_BUDGET`) from
  explicit backtrack-workspace exhaustion (`RE_SPACE`).
- Captures use caller-provided pointer slots. A stack snapshot at each split restores captures
  when backtracking, including unset and keep-last semantics.
- Both committed corpora are green against `src/re.c`: seed1 passes 1955/1955 with 382 capture
  spans checked, and seed7 passes 2872/2872 with 533 capture spans checked. Nullable repeats and deliberate budget
  exhaustion cases are reported separately rather than counted as semantic matches.

## Known gaps

- Multi-code-point case folds such as `ß` to `ss` are deliberately not expanded.
- Relative jumps use signed 16-bit offsets; compiled programs larger than 32 KiB are rejected.
- UTF-8 decoding is permissive. Truncated input does not read beyond the terminating NUL, but
  malformed continuation bytes are not validated.
- The adapter is test-only and is not included in the VM size measurement.

See `BUILD.txt` for reproducible measurement and corpus commands.
