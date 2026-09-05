# Semantics

mini-regexp uses Perl/Python-style leftmost-first backtracking semantics.

- `.` and character classes consume one UTF-8 code point; offsets and captures are byte pointers.
- `.` excludes newline unless the compiler's `s` flag is enabled.
- `^` and `$` match only the absolute start and end in v1. The compiler also supports the future
  multiline `m` flag.
- `\d`, `\w`, `\s`, their uppercase complements, and `\b`/`\B` use ASCII definitions.
- `i` expands Unicode simple case-fold equivalents in compiled character classes. A fold that
  expands to multiple code points, such as `ß` to `ss`, is not supported.
- Alternation and quantifiers are leftmost-first; quantifiers are greedy unless followed by `?`.
- Repeated captures keep the last successful iteration. Captures abandoned by backtracking are
  restored, and unmatched groups are null.
- Unbounded repetition of a nullable atom is rejected by the compiler.
- Exhausting the caller-provided step budget returns `RE_BUDGET`; exhausting explicit backtrack
  workspace returns `RE_SPACE`. Matching never recurses on the C stack.
- Malformed UTF-8 is consumed permissively. Truncated input never reads beyond its terminating
  NUL, but continuation bytes are not fully validated.

Backreferences, lookaround, named groups, POSIX classes and multi-code-point case folding are out
of scope for v1.
