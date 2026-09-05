/* re.h -- tiny backtracking regex engine, direct pattern interpretation (no compile step).
 *
 * Syntax: literals (UTF-8), . ^ $, \d \D \w \W \s \S (ASCII), \b \B (ASCII \w based), \n \t \r,
 * \<any non-alphanumeric char> = that char, [...] [^...] with ranges by code point and class
 * escapes, * + ? {n} {n,} {n,m} {,m} (each optionally lazy with a trailing ?), alternation |,
 * groups (...) and (?:...). Anything else (\1, \A, \x41, (?i), lookaround, possessive *+)
 * is a pattern error, never silently something else.
 * Semantics: Perl/JS/Python leftmost-first backtracking search; captures report the last
 * iteration of a repeated group; an iteration that matches empty ends the loop; ^ and $ are
 * text anchors ($ does NOT match before a final newline, unlike Python; no multiline mode
 * unless built with -DRE_MULTILINE and RE_MULTI is passed in flags); \B never matches in an
 * empty text (Python). Offsets are byte offsets into the UTF-8 text; malformed UTF-8 bytes
 * count as one code point each.
 */
#ifndef RE_H
#define RE_H

#define RE_ICASE 1u   /* ASCII-only case-insensitive matching */
#define RE_MULTI 2u   /* multiline ^/$ (only if built with -DRE_MULTILINE) */

enum {
    RE_MATCH   =  1,  /* matched; caps filled in */
    RE_NOMATCH =  0,
    RE_ERROR   = -1,  /* malformed pattern */
    RE_LIMIT   = -2   /* gave up: backtracking step budget exhausted (RE_STEP_LIMIT, default 10M;
                         catastrophic patterns such as ^(a+)+$ on a non-matching text), or the
                         caller's workspace was too small for this pattern and text */
};

/* Scratch bytes for a match whose deepest search path holds `iters` group iterations at
 * once -- one per iteration of every quantified group open at that moment, so the text
 * length is the honest bound for a pattern like (a)* . Eight pointer-sized words per
 * iteration always covers the iteration record and the capture writes that go with it.
 * Patterns with no groups need none at all. */
#define RE_WORKSPACE_BYTES(iters) ((unsigned)(iters) * 8u * (unsigned)sizeof(void *))

/* Search for pat anywhere in text (both NUL-terminated).
 * caps holds ncaps (start,end) pairs of BYTE offsets: caps[0],caps[1] = whole match,
 * caps[2k],caps[2k+1] = group k. Unset groups are -1,-1. Groups beyond ncaps are matched
 * but not reported. caps may be NULL when ncaps is 0.
 * ws/wsbytes is caller-owned scratch for group iteration and capture-undo records; it is
 * the only thing that grows with the text, the C stack cost of a match depends on how
 * deeply the pattern nests groups and not on how many times they repeat. ws must be
 * suitably aligned for pointers and wsbytes a multiple of sizeof(void *); ws may be NULL
 * when wsbytes is 0. Too small a workspace gives RE_LIMIT, never a wrong answer.
 * Reentrant; no allocation, no libc. */
int re_match(const char *pat, const char *text, unsigned flags, int *caps, int ncaps,
             void *ws, unsigned wsbytes);

#endif
