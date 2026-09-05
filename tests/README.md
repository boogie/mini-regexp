# Tests

The test data is language-agnostic on purpose: it must stay usable whether the final design is a
single C engine or a compiler (possibly in JavaScript) plus a C matcher. Only the corpus format and
the CLI contract are fixed; the runner is not decided yet.

## Corpus format

`tests/corpus/*.jsonl`: one JSON object per line.

```json
{"flags": "", "pat": "(a|ab)(c|bcd)(d*)", "text": "abcd",
 "expect": {"span": [0, 4], "groups": [[0, 1], [1, 4], [4, 4]]}}
```

- `flags`: `""` or `"i"` (Unicode simple case-insensitive, expanded by the compiler).
- `pat`, `text`: UTF-8 strings.
- `expect`: `null` for no match; `"error"` when the oracle rejects the pattern (the engine may
  reject or accept it, but must not crash or hang); otherwise the overall match span and one
  `[start, end]` per capturing group (`[-1, -1]` for an unset group). **All offsets are byte
  offsets into the UTF-8 text, half-open.**
- `hand: true` marks hand-written edge cases; the rest is generated.
- `nullable_repeat: true` marks patterns that repeat a nullable atom (`(a*)*`, `(|a)+`, `()+`).
  These are **out of scope**: an engine may reject them with an error, or match them correctly.
  Either counts as a pass; hanging or crashing does not.
- `budget_exhaustion: true` marks adversarial patterns that may return the step-budget error.
  A match or no-match result is also accepted; hanging or crashing is not.

The base oracle is Python 3 `re` with `re.ASCII` for Perl/Python leftmost-first semantics and
ASCII shorthand classes. `annotate_corpus.js` applies project-specific nullable/budget metadata
and the Unicode-`i` expectations that cannot be expressed by that single Python flag. Regenerate:

```
python3 tools/gen_corpus.py --wb 1 2000 > tests/corpus/corpus_wb_seed1.jsonl
node tools/annotate_corpus.js tests/corpus/corpus_wb_seed1.jsonl
```

## CLI contract

Any engine under test provides a small host-compiled program:

```
./engine_cli [-i] PATTERN TEXT
```

stdout line 1: `match START END` (bytes) | `nomatch` | `error`; optional following lines
`N START END` for capturing group N (1-based, `-1 -1` if unset). Then:

```
python3 tools/difftest.py ./engine_cli tests/corpus/corpus_wb_seed1.jsonl
```

The compiler + VM adapter also supports a persistent mode that builds the Unicode fold map once:

```
python3 tools/difftest.py --batch research/spikes/compiler-vm/corpus_adapter.js \
  tests/corpus/corpus_wb_seed1.jsonl
```

A case that runs longer than 2 seconds counts as a failure (hang / catastrophic backtracking).

## Open questions (runner)

- If the design becomes "JS compiler + C matcher", the CLI wrapper will run the compiler first
  (Node) and feed its output to the C matcher; the corpus and `difftest.py` stay unchanged.
- Unit tests for the C code (table-driven, `-fsanitize=address,undefined`) and the fuzz harness
  are planned but not set up; see `docs/feasibility.md` §7 for the intended strategy.
