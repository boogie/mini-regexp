/* re.c -- direct-interpretation backtracking regex engine (Kernighan/Pike "matchhere" lineage).
 *
 * There is no compile step: the matcher walks the raw pattern string. The only work done up
 * front is a one-pass syntax check, and that pass is not its own function: walk() validates
 * the pattern when it is asked to run to the end and skips over it when the matcher asks it
 * to find the end of an alternative or of a group body. So the matcher can assume a
 * well-formed pattern and never has to report errors.
 *
 * How the pieces fit together
 *   walk()       one pattern scanner, three jobs (validate / skip one alternative / skip a
 *                whole group body), plus counting capturing '(' up to a limit for cap_index().
 *   atom()       one single-character construct: '.', a bracket class, a literal code point or
 *                an escape. A bare item is the degenerate class body of one member, so class
 *                and item run the same membership loop. It also doubles as the "how long is
 *                this construct" primitive every scanner needs (st->pend).
 *   match()      walks one alternative of a sequence, item by item, consuming text. When it
 *                reaches the end of the alternative ('|', ')' or NUL) the *continuation*
 *                decides what happens next: the innermost open group's frame.
 *   re_frame     one per group *nesting level* (not per iteration), on the C stack of group().
 *                It carries only what the end of a body alternative has to touch: which
 *                solution of the body group() is waiting for, and where that solution ended.
 *   group()      the whole iteration loop of a quantified group, and it does NOT recurse per
 *                iteration: the chain of completed iterations lives in the caller-supplied
 *                workspace as re_iter records, so the C stack cost of (a)* is the same for
 *                8 and for 8000 a's. Its inner move is "match the body and stop on its j'th
 *                solution". That is what replaces the recursive continuation: instead of
 *                keeping iteration i's search alive on the C stack while iteration i+1 runs,
 *                only the ordinal j of the solution used is remembered, and backtracking into
 *                that iteration re-derives it by re-running the body. Same time-for-stack
 *                trade as repeat(), and allowed for the same reason: here time is free and
 *                stack is not.
 *   group_end()  called when a body alternative reached its end: counts one solution of the
 *                body and accepts only the one group() asked for.
 *   repeat()     single-character atoms with a quantifier: greedy or lazy, in constant stack
 *                (positions are re-derived by walking forward from the start, with the same
 *                loop that counted the repetitions in the first place).
 *   The whole pattern is itself matched as a group with quantifier {1} whose capture is
 *   group 0, so top-level alternation and the overall span fall out of the same code.
 *
 * Workspace: re_match() takes a scratch buffer from the caller, and that is the only thing
 * that grows with the text. It holds two stacks growing towards each other: re_iter records
 * from the bottom (one per group iteration currently on the match path) and capture undo
 * records from the top. A group writes its capture as soon as it hands a solution to its
 * caller, so an enclosing backtrack has to be able to take that write back; the undo record
 * is how. Running out of workspace is reported like the step budget (RE_LIMIT), never as a
 * wrong answer. ws must be int-aligned; see RE_WORKSPACE_BYTES in re.h for the sizing rule.
 *
 * Runaway protection: backtracking is exponential for patterns like ^(a+)+$ (as it is in
 * Python and Perl). Every match() step decrements a budget of RE_STEP_LIMIT; when it runs out
 * re_match() gives up and returns RE_LIMIT instead of stalling the MCU. There is no memory
 * to memoise with, so this is the whole defence.
 *
 * Feature toggles (each removes code; see SIZE.txt / NOTES.md for the byte ledger):
 *   RE_NO_LAZY RE_NO_BOUNDED RE_NO_CAPTURES RE_NO_UTF8 RE_NO_ICASE RE_NO_WORDB RE_NO_CLASSES
 *   RE_NO_ALT (no groups, no alternation; the workspace is then unused) RE_NO_BUDGET (no
 *   step counter, RE_LIMIT still returned when the workspace runs out) RE_MULTILINE (adds
 *   the m flag) RE_ALLOW_INLINE (smaller code, bigger stack frames) RE_STEP_LIMIT=n (budget,
 *   default 10 million) RE_MAX_DEPTH=n (optional guard on group *nesting*, which is the only
 *   thing that still costs C stack; a pattern nested deeper than n gives RE_LIMIT)
 */
#include <stddef.h>
#include "re.h"

#ifndef RE_STEP_LIMIT
#define RE_STEP_LIMIT 10000000   /* match() steps per re_match() call before giving up with RE_LIMIT */
#endif
#define RE_INF 0x7fffffff       /* "no upper bound"; counts of 9+ digits in {n,m} clamp to it */

/* group() is called from one place, so gcc would inline it into match() and every match()
 * frame would then carry all of group()'s locals. Keeping it separate makes the per-nesting
 * level stack smaller. (repeat() is deliberately *not* marked: inlining it into match()
 * turned out to be both smaller and cheaper in stack than a separate frame.) */
#if defined(__GNUC__) && !defined(RE_ALLOW_INLINE)
#define RE_NOINLINE __attribute__((noinline))
#else
#define RE_NOINLINE
#endif

typedef struct re_quant {
    int min, max;
#ifndef RE_NO_LAZY
    int lazy;
#endif
} re_quant;

#ifdef RE_NO_LAZY
#define RE_LAZY(q) 0
#else
#define RE_LAZY(q) ((q)->lazy)
#endif

/* Undo record for one capture-array word, pushed from the top of the workspace downwards. */
typedef struct re_cap { int *p, old; } re_cap;

/* One group iteration on the current match path, pushed from the bottom of the workspace
 * upwards. This is the state the C stack used to hold per iteration: where the iteration
 * started, which solution of the body it took, and how far the capture undo log had grown
 * before it, so that abandoning it puts the captures back. */
typedef struct re_iter {
    int j;
    const char *s;
    struct re_cap *h;
} re_iter;

typedef struct re_state {
    const char *pat;    /* pattern start: cap_index() counts groups from here */
    const char *text;   /* text start: '^', \b and capture offsets refer to it */
    int *caps;          /* caller's capture array, 2 ints per group */
#ifndef RE_NO_ALT
    re_iter *lo;        /* next free iteration record; group() restores it on the way out */
    re_cap *hi;         /* lowest capture undo record in use */
#endif
#if !defined(RE_NO_ALT) && !defined(RE_NO_CAPTURES)
    int ncap;           /* capturing '(' seen by the last walk() that ran to a limit */
#endif
    const char *pend;   /* pattern position after the atom last scanned by atom() */
    int ncaps;          /* number of (start,end) pairs in caps */
    unsigned flags;
#ifndef RE_NO_BUDGET
    int steps;          /* remaining match() steps; negative once exhausted */
#endif
#ifdef RE_NO_BUDGET
    int over;           /* set when the workspace ran out (the budget carries it otherwise) */
#endif
#ifdef RE_MAX_DEPTH
    int depth;          /* groups currently open on the C stack */
#endif
#ifdef RE_NO_ALT
    const char *mend;   /* end of the match (no frames exist in this build) */
#endif
} re_state;

#ifndef RE_NO_BUDGET
#define RE_ABORT(st)   ((st)->steps = -1)   /* out of workspace: spend the budget, fail fast */
#define RE_ABORTED(st) ((st)->steps < 0)
#else
#define RE_ABORT(st)   ((st)->over = 1)
#define RE_ABORTED(st) ((st)->over)
#endif

/* The continuation seen by match(): the innermost open group, reduced to the only two things
 * the end of an alternative has to touch. Everything else about the group (body, quantifier,
 * capture, iteration count) stays in group()'s own registers, one set per nesting level. */
typedef struct re_frame {
    const char *end;      /* text end of the body solution group() asked for */
    int want;             /* solutions still to be skipped before one is accepted */
} re_frame;

/* ------------------------------------------------------------------ characters */

static int is_digit(int c) { return c >= '0' && c <= '9'; }

static int is_alnum(int c) { return is_digit(c) || ((c | 0x20) >= 'a' && (c | 0x20) <= 'z'); }

static int is_word(int c) { return is_alnum(c) || c == '_'; }   /* ASCII \w */

/* Decode the UTF-8 code point at s. A byte that does not begin a complete sequence is
 * returned as itself, so malformed input advances one byte at a time. Relies on NUL
 * termination: a continuation byte is never 0. */
static int decode(const char *s, const char **next)
{
    const unsigned char *u = (const unsigned char *)s;
    int c = *u++;
#ifndef RE_NO_UTF8
    if (c >= 0xC0) {
        int n = 1 + (c >= 0xE0) + (c >= 0xF0);       /* continuation bytes expected */
        int v = c & (0x3F >> n);
        const unsigned char *t = u;
        while (n-- > 0 && (*t & 0xC0) == 0x80) v = (v << 6) | (*t++ & 0x3F);
        if (n < 0) { c = v; u = t; }                  /* complete sequence */
    }
#endif
    *next = (const char *)u;
    return c;
}

/* The other-case twin of an ASCII letter when matching case-insensitively, else c. */
static int other_case(const re_state *st, int c)
{
#ifndef RE_NO_ICASE
    if ((st->flags & RE_ICASE) && (c | 0x20) >= 'a' && (c | 0x20) <= 'z') c ^= 0x20;
#else
    (void)st;
#endif
    return c;
}

/* Membership of code point c in a class escape: e is one of d D w W s S. */
static int class_escape(int e, int c)
{
    int lo = e | 0x20, r;
    if (lo == 'd') r = is_digit(c);
    else if (lo == 'w') r = is_word(c);
    else r = c == ' ' || (c >= '\t' && c <= '\r');
    return (e & 0x20) ? r : !r;                       /* upper-case letter negates */
}

/* Read one pattern character at *pp: a code point, or an escape. \n \t \r give the control
 * character, \d \D \w \W \s \S give the negated letter as a class-escape marker, any other
 * escaped punctuation or non-ASCII character gives itself. Returns -1 (RE_BAD) at the end of
 * the pattern and for escapes of other letters and digits; never reads past the NUL. */
#define RE_BAD (-1)
static int pat_char(const char **pp)
{
    const char *p = *pp;
    int c, esc;
    for (esc = 0; ; esc = 1) {           /* one decode site serves both the char and the escape */
        c = *p ? decode(p, &p) : RE_BAD;
        if (esc || c != '\\') break;
    }
    if (esc && is_alnum(c)) {            /* escaped punctuation and non-ASCII give themselves */
        int lo = c | 0x20;
        if (lo == 'd' || lo == 'w' || lo == 's') c = -c;
        else if (c == 'n') c = '\n';
        else if (c == 't') c = '\t';
        else if (c == 'r') c = '\r';
        else c = RE_BAD;                 /* escape of an unknown letter or a digit */
    }
    *pp = p;
    return c;
}

/* Does the code-point range lo..hi (a single item when hi == lo, a class escape when lo < 0)
 * match code point c or its other-case twin c2? One test serves the bare atom, a class
 * member and a class range alike. */
static int item_hit(int lo, int hi, int c, int c2)
{
    if (lo < 0) return class_escape(-lo, c);
    return (lo <= c && c <= hi) || (lo <= c2 && c2 <= hi);
}

/* Match the single-character atom at p against the text at s: '.', a bracket class
 * [...] / [^...], or a single item (a literal code point, an escape, or a class escape).
 * A bare item is the degenerate class body of one member that ends after it, so one loop
 * serves both and there is a single pat_char()/item_hit() pair. Always stores the pattern
 * position after the atom in *pend (NULL on a class syntax error); returns the text position
 * after the matched character, or NULL. At the end of the text decode() yields 0, which no
 * pattern item can equal, and the final test turns the "hit" of a negated class into a miss.
 * A ']' directly after '[' or '[^' is a literal. */
static const char *atom(re_state *st, const char *p, const char *s)
{
    int c = decode(s, &s);
    int c2 = other_case(st, c);
    int hit = 0;
#ifndef RE_NO_CLASSES
    int neg = 0, cls = *p == '[';
#else
    enum { cls = 0 };
#endif
    if (*p == '.') { p++; hit = c != '\n'; }
    else {
#ifndef RE_NO_CLASSES
        if (cls) { p++; if (*p == '^') { neg = 1; p++; } }
#endif
        for (;;) {
            int lo = pat_char(&p), hi = lo;
            if (lo == RE_BAD) { p = 0; break; }        /* bad escape, or unterminated class */
            if (cls) {
                if (*p == '-' && p[1] != ']') {        /* range lo-hi by code point */
                    p++;
                    hi = pat_char(&p);
                    if (lo < 0 || hi < 0 || lo > hi) { p = 0; break; }
                }
            }
            if (item_hit(lo, hi, c, c2)) hit = 1;
            if (!cls) break;
#ifndef RE_NO_CLASSES
            if (*p == ']') { hit = hit != neg; p++; break; }
#endif
        }
    }
    st->pend = p;
    return hit && c ? s : NULL;
}

/* ------------------------------------------------------------------ pattern scanning */

/* Parse the quantifier at p, if any, into q. Returns the position after it (p itself when
 * there is none, so a '{' that does not form {n}, {n,}, {n,m} or {,m} stays a literal), or
 * NULL for {n,m} with n > m. */
static const char *parse_quant(const char *p, re_quant *q)
{
    int c = *p;
    q->min = q->max = 1;
#ifndef RE_NO_LAZY
    q->lazy = 0;
#endif
#ifndef RE_NO_BOUNDED
    if (c == '{') {
        /* One digit loop for both bounds: n accumulates the number being read and a comma
         * parks the first one in lo, so {n}, {n,}, {n,m} and {,m} all fall out of it.
         * Values that do not fit comfortably in an int clamp to RE_INF (they exceed any
         * text length anyway). */
        const char *t = p;
        int d, lo = -1, n = -1;        /* -1: no digits in the field being filled */
        while (is_digit(d = *++t) || (d == ',' && lo < 0)) {
            if (d == ',') { lo = n < 0 ? 0 : n; n = -1; }
            else n = n < 0 ? d - '0' : n < 100000000 ? n * 10 + (d - '0') : RE_INF;
        }
        if (d != '}' || t == p + 1) return p;    /* "{}" and "{x" stay literal */
        if (lo < 0) lo = n;            /* {n}: no comma, so max == min */
        else if (n < 0) n = RE_INF;    /* {n,}: nothing after the comma */
        if (lo > n) return NULL;
        q->min = lo;
        q->max = n;
        p = t;
    } else
#endif
    if (c == '*' || c == '+' || c == '?') {
        q->min = c == '+';
        q->max = c == '?' ? 1 : RE_INF;
    } else return p;
    p++;
#ifndef RE_NO_LAZY
    if (*p == '?') { q->lazy = 1; p++; }
#endif
    return p;
}

/* One walker, two jobs -- there is no separate validation pass. stop=0 validates the whole
 * pattern and returns non-NULL exactly when it is well formed (unbalanced parentheses, (?
 * other than (?:, a quantifier with nothing to repeat or stacked on another, bad {n,m},
 * escapes of unknown letters, unterminated classes and bad ranges all give NULL). stop=1
 * skips to the '|' or ')' ending this alternative at the same nesting depth, stop=2 to the
 * ')' ending the group; both also stop at the terminating NUL. The matcher only ever uses
 * stop > 0, on a pattern that has already validated, so NULL never comes back to it. */
static const char *walk(re_state *st, const char *p, int stop, const char *lim)
{
    int depth = 0;
    for (;;) {
        int c = *p;
        re_quant q;
        if (c == 0) return depth ? NULL : p;
#ifndef RE_NO_ALT
        if (p == lim) return p;            /* cap_index()'s stop: count only, up to here */
        if (stop && !depth && (c == ')' || (c == '|' && stop < 2))) return p;
        if (c == '(') {
            depth++;
            p++;
            if (*p == '?') { if (p[1] != ':') return NULL; p += 2; }
#ifndef RE_NO_CAPTURES
            else st->ncap++;
#endif
            continue;
        }
        if (c == '|') { p++; continue; }
#else
        (void)stop;
        (void)lim;
        if (c == '(' || c == ')' || c == '|') return NULL;
#endif
        if (c == '^' || c == '$') { p++; continue; }
#ifndef RE_NO_WORDB
        if (c == '\\' && (p[1] | 0x20) == 'b') { p += 2; continue; }
#endif
        /* every atom below consumes its own quantifier, so one here has nothing to repeat
         * (and parse_quant() returning NULL catches {n,m} with n > m at the same time) */
        if (parse_quant(p, &q) != p) return NULL;
#ifndef RE_NO_ALT
        if (c == ')') {
            if (depth-- == 0) return NULL;
            p++;
        } else
#endif
        {
#ifdef RE_NO_CLASSES
            if (c == '[') return NULL;
#endif
            atom(st, p, st->text);   /* any readable text does: only st->pend is used */
            if (!st->pend) return NULL;
            p = st->pend;
        }
        p = parse_quant(p, &q);
        if (!p) return NULL;
    }
}

#ifndef RE_NO_ALT
#define skip_alt(st, p) walk((st), (p), 1, 0)
#define skip_group(st, p) walk((st), (p), 2, 0)

#ifndef RE_NO_CAPTURES
/* Capture index of the group whose '(' precedes p: the same walk over the pattern, stopped
 * at p, counting capturing '(' on the way. */
static int cap_index(re_state *st, const char *p)
{
    st->ncap = 0;
    walk(st, st->pat, 2, p);
    return st->ncap;
}
#endif
#endif /* RE_NO_ALT */

/* ------------------------------------------------------------------ matching */

static int match(re_state *st, const char *p, const char *s, re_frame *f);

#ifndef RE_NO_WORDB
/* Does the assertion \b (want_boundary=1) or \B (0) hold at s? Python quirk kept on purpose:
 * neither matches in an empty text (Perl/JS let \B match there). */
static int word_assert(const re_state *st, const char *s, int want_boundary)
{
    int before = s > st->text && is_word((unsigned char)s[-1]);
    int after = is_word((unsigned char)*s);
    return (before ^ after) == want_boundary && *st->text;   /* both are 0/1 */
}
#endif

/* Quantified single-character atom at p; pn is the pattern after the quantifier.
 * Greedy: take as many as possible and give back one at a time. Lazy: take the minimum and
 * add one at a time. Constant stack: the position for k repetitions is never remembered,
 * it is re-derived by walking k atoms forward from s again -- and that walk is the same
 * loop that counted them in the first place, entered with k = q->max (i < 0 marks that
 * counting pass). Time is free here; bytes are not. */
static int repeat(re_state *st, const char *p, const char *s, const char *pn,
                              const re_quant *q, re_frame *f)
{
    const char *t, *u;
    int j, i = -1, k = q->max, step = -1;
    for (;;) {
        u = s;                             /* walk k atoms forward from s */
        for (j = 0; j < k && (t = atom(st, p, u)) != NULL; j++) u = t;
        if (i < 0) {                       /* first pass: k was q->max, so j is the most */
            i = j - q->min;                /* how many counts are still on the table */
            if (i < 0) return 0;
            k = j;                         /* greedy starts at the most and counts down */
            if (RE_LAZY(q)) { k = q->min; step = 1; continue; }
        }
        if (match(st, pn, u, f)) return 1;
        if (--i < 0) return 0;
        k += step;
    }
}

#ifndef RE_NO_ALT
/* Store the offset of v in the capture word *p, keeping an undo record on the workspace so
 * that a later backtrack of an enclosing group can put the old value back. A group hands a
 * solution to its caller long before the whole match is decided, so its capture write is not
 * final and cannot simply be left in place. */
static RE_NOINLINE void save_pos(re_state *st, int *p, const char *v)
{
    re_cap *u = st->hi - 1;
    if ((char *)u < (char *)st->lo) { RE_ABORT(st); return; }
    st->hi = u;
    u->p = p;
    u->old = *p;
    *p = (int)(v - st->text);
}

/* An alternative of group f's body reached its end at s: that is one solution of the body.
 * group() only wants the f->want'th one; every earlier one is refused, so match() keeps
 * backtracking and produces the next. This one test is the whole continuation. */
static int group_end(re_frame *f, const char *s)
{
    if (f->want--) return 0;
    f->end = s;
    return 1;
}

/* Match the group whose body starts at body (just past '(' or '(?:'), recording capture cap
 * (RE_INF: none), then whatever follows the group. The whole pattern is matched the same way:
 * its body ends at the NUL instead of a ')', and there is no quantifier after it.
 *
 * The iteration loop is flat. j is the move this state makes next: a solution ordinal of the
 * body, or negative for "leave the group and match what follows". Greedy tries iterating
 * first and leaving second, lazy the other way round; ~j flips between the two. Every taken
 * iteration pushes an re_iter record, and backtracking pops one and asks the body for its
 * next solution instead of resuming a live recursive search. */
static RE_NOINLINE int group(re_state *st, const char *body, const char *s, re_frame *up, int cap)
{
    re_frame f;
    re_quant q;
    re_iter *base = st->lo;
    const char *next, *p = cap < 0 ? st->pend : skip_group(st, body);
    int done = 0, j, forced, r = 1;
    if (cap >= 0 && *p) p++;
    next = parse_quant(p, &q);
    j = -RE_LAZY(&q);
#ifdef RE_MAX_DEPTH
    if (++st->depth > RE_MAX_DEPTH) RE_ABORT(st);
#endif
    for (;;) {
        forced = done < q.min;
        /* j < 0 is the "leave the group" move; j >= 0 asks the body for its j'th solution.
         * An iteration that matched the empty string ends the loop (Python's rule), except
         * for iterations forced by the minimum count, which Python never checks: the last
         * iteration started at st->lo[-1].s, and the done <= q.min test in front of that read
         * covers both "no iteration yet" and "the last one was forced", so the record it
         * looks at always exists. */
        if (j < 0) {
            if (!forced && match(st, next, s, up)) break;
        } else if (forced || (done < q.max && (done <= q.min || st->lo[-1].s != s))) {
            /* Match the body and stop on its j'th solution, leaving its end in f.end.
             * Alternatives are tried in order, so solutions come out leftmost-first: the
             * same order the recursive continuation used to visit them in. */
            re_cap *h = st->hi;
            const char *bp = body;
            f.want = j;
            if (cap < 0) {
                if (!j && (f.end = atom(st, body, s))) goto took;
                goto exhausted;
            }
            do {
                if (match(st, bp, s, &f)) goto took;
                bp = skip_alt(st, bp);
            } while (*bp++ == '|');
            goto exhausted;
        took:
            if ((char *)(st->lo + 1) > (char *)st->hi) { RE_ABORT(st); r = 0; goto out; }
            st->lo->s = s;
            st->lo->h = h;
            st->lo->j = j;
            st->lo++;
            s = f.end;
            done++;
            j = -RE_LAZY(&q);
            continue;
        }
    exhausted:
        if (RE_LAZY(&q) == (j < 0)) { j = ~j; continue; }   /* the other move of this state */
        if (st->lo == base) { r = 0; goto out; }   /* no iteration left to undo: the group fails */
        --st->lo;
        while (st->hi < st->lo->h) { *st->hi->p = st->hi->old; st->hi++; }   /* undo captures */
        s = st->lo->s;
        j = st->lo->j + 1;
        done--;
    }
    /* Won: the group's capture is its last iteration (nothing if it ran zero times). Group 0
     * reports the whole match through the same two words in every build. */
    if (done && cap >= 0 && cap < st->ncaps) {
        int *cp = st->caps + 2 * cap;
        save_pos(st, cp, st->lo[-1].s);
        save_pos(st, cp + 1, s);
    }
out:
#ifdef RE_MAX_DEPTH
    st->depth--;
#endif
    st->lo = base;
    return r;
}

/* Enter the group whose '(' is at p from inside match(). */
static int enter_group(re_state *st, const char *p, const char *s, re_frame *up)
{
    int cap = RE_INF;                      /* below no ncaps: a (?: records nothing */
    p++;
    if (*p == '?') p += 2;                 /* (?: non-capturing */
#ifndef RE_NO_CAPTURES
    else cap = cap_index(st, p);
#endif
    return group(st, p, s, up, cap);
}
#endif /* RE_NO_ALT */

/* Match one alternative of a sequence starting at pattern p against the text at s.
 * f is the frame of the innermost open group (the continuation). */
static int match(re_state *st, const char *p, const char *s, re_frame *f)
{
    for (;;) {
        int c = *p;
        const char *t, *pe, *pn;
        re_quant q;
#ifndef RE_NO_BUDGET
        if (--st->steps < 0) return 0;     /* budget gone: fail fast all the way up */
#endif
#ifdef RE_NO_ALT
        (void)f;
        if (c == 0) { st->mend = s; return 1; }
#else
        if (c == 0 || c == '|' || c == ')') return f ? group_end(f, s) : 1;
        if (c == '(') return enter_group(st, p, s, f);
#endif
        if (c == '^' || c == '$') {         /* one anchor test: '^' looks back, '$' looks ahead */
            const char *a = c == '^' ? s - (s != st->text) : s;
            if ((c == '^' ? a != s : *a != 0)
#ifdef RE_MULTILINE
                && !((st->flags & RE_MULTI) && *a == '\n')
#endif
                ) return 0;
            p++;
            continue;
        }
#ifndef RE_NO_WORDB
        if (c == '\\' && (p[1] | 0x20) == 'b') {
            if (!word_assert(st, s, p[1] == 'b')) return 0;
            p += 2;
            continue;
        }
#endif
        /* single-character atom, possibly quantified */
        t = atom(st, p, s);
        pe = st->pend;
        pn = parse_quant(pe, &q);
        if (pn != pe) return group(st, p, s, f, -1);
        if (!t) return 0;
        p = pn;
        s = t;
    }
}

/* ------------------------------------------------------------------ validation and entry */

int re_match(const char *pat, const char *text, unsigned flags, int *caps, int ncaps,
             void *ws, unsigned wsbytes)
{
    re_state st;
    const char *s = text;
    volatile int *unset = caps;            /* volatile: keeps gcc from calling memset here */
    int i;
    st.pat = pat;
    st.text = text;
    st.caps = caps;
    st.ncaps = ncaps;
    st.flags = flags;
    if (!walk(&st, pat, 0, 0)) return RE_ERROR;
    for (i = 0; i < 2 * ncaps; i++) unset[i] = -1;
#ifndef RE_NO_BUDGET
    st.steps = RE_STEP_LIMIT;
#else
    st.over = 0;
#endif
#ifdef RE_MAX_DEPTH
    st.depth = 0;
#endif
#ifndef RE_NO_ALT
    st.lo = (re_iter *)ws;                 /* group() leaves both ends where it found them */
    st.hi = (re_cap *)((char *)ws + wsbytes);
#endif
    for (;;) {                             /* leftmost match: try each code point boundary */
#ifdef RE_NO_ALT
        (void)ws; (void)wsbytes;
        i = match(&st, pat, s, 0);
        if (i && ncaps > 0) { caps[0] = (int)(s - text); caps[1] = (int)(st.mend - text); }
#else
        i = group(&st, pat, s, 0, 0);      /* the pattern is group 0 */
#endif
        /* The abort test comes first: a workspace that ran out while the captures were being
         * written leaves them half done, and that has to be RE_LIMIT, not a bad RE_MATCH. */
        if (RE_ABORTED(&st)) return RE_LIMIT;
        if (i) return RE_MATCH;
        if (*s == 0) return RE_NOMATCH;
        decode(s, &s);
    }
}
