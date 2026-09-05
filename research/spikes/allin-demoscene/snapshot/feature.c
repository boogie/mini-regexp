/* feature.c -- explicit per-feature liveness check for the shrunk engine.
 * Each row: feature label, flags, pattern, text, expected return, expected span,
 * and (optionally) expected group-1 / group-2 spans (-2 = don't care).
 * Each row carries its own hardcoded expected return, span and group spans. */
#include <stdio.h>
#include <string.h>
#include "re.h"


struct row { const char *feat, *pat, *text; unsigned fl; int r, s, e, g1s, g1e, g2s, g2e; };

#define DC -2
static const struct row rows[] = {
/* feature              pattern            text          flags     r  s  e  g1s g1e g2s g2e */
{"literal ASCII",       "abc",             "xxabcyy",    0,        1, 2, 5, DC,DC,DC,DC},
{"literal no-match",    "abc",             "xxabyy",     0,        0, DC,DC,DC,DC,DC,DC},
{"dot",                 "a.c",             "a-c",        0,        1, 0, 3, DC,DC,DC,DC},
{"dot excludes NL",     "a.c",             "a\nc",       0,        0, DC,DC,DC,DC,DC,DC},
{"anchor ^",            "^ab",             "abc",        0,        1, 0, 2, DC,DC,DC,DC},
{"anchor ^ neg",        "^bc",             "abc",        0,        0, DC,DC,DC,DC,DC,DC},
{"anchor $",            "bc$",             "abc",        0,        1, 1, 3, DC,DC,DC,DC},
{"anchor $ neg",        "ab$",             "abc",        0,        0, DC,DC,DC,DC,DC,DC},
{"anchor ^...$",        "^abc$",           "abc",        0,        1, 0, 3, DC,DC,DC,DC},
{"esc \\d",             "\\d\\d",          "ab42cd",     0,        1, 2, 4, DC,DC,DC,DC},
{"esc \\D",             "\\D",             "12x",        0,        1, 2, 3, DC,DC,DC,DC},
{"esc \\w",             "\\w+",            "  ab_9 ",    0,        1, 2, 6, DC,DC,DC,DC},
{"esc \\W",             "\\W",             "ab-c",       0,        1, 2, 3, DC,DC,DC,DC},
{"esc \\s",             "a\\sb",           "a\tb",       0,        1, 0, 3, DC,DC,DC,DC},
{"esc \\S",             "\\S",             "   q",       0,        1, 3, 4, DC,DC,DC,DC},
{"esc \\n \\t \\r",     "a\\n\\t\\rb",     "a\n\t\rb",   0,        1, 0, 5, DC,DC,DC,DC},
{"esc literal \\.",     "a\\.c",           "a.c",        0,        1, 0, 3, DC,DC,DC,DC},
{"esc literal \\. neg", "a\\.c",           "abc",        0,        0, DC,DC,DC,DC,DC,DC},
{"word bound \\b",      "\\bcat\\b",       "a cat!",     0,        1, 2, 5, DC,DC,DC,DC},
{"word bound \\b neg",  "\\bcat\\b",       "concat",     0,        0, DC,DC,DC,DC,DC,DC},
{"non-bound \\B",       "\\Bcat",          "concat",     0,        1, 3, 6, DC,DC,DC,DC},
{"non-bound \\B neg",   "\\Bcat",          "a cat",      0,        0, DC,DC,DC,DC,DC,DC},
{"class literal",       "[xyz]",           "aby",        0,        1, 2, 3, DC,DC,DC,DC},
{"class range",         "[a-f]+",          "zzbead9",    0,        1, 2, 6, DC,DC,DC,DC},
{"class negated",       "[^a-f]",          "abcx",       0,        1, 3, 4, DC,DC,DC,DC},
{"class w/ esc",        "[\\d-]+",         "aa-42b",     0,        1, 2, 5, DC,DC,DC,DC},
{"class ] first",       "[]a]+",           "x]aa",       0,        1, 1, 4, DC,DC,DC,DC},
{"class neg range",     "[^0-9]+",         "12abc3",     0,        1, 2, 5, DC,DC,DC,DC},
{"utf8 literal",        "\xc3\xa9",        "ab\xc3\xa9", 0,        1, 2, 4, DC,DC,DC,DC},
{"utf8 3-byte",         "\xe6\x97\xa5",    "x\xe6\x97\xa5y",0,     1, 1, 4, DC,DC,DC,DC},
{"utf8 4-byte",         "\xf0\x9f\x98\x80","a\xf0\x9f\x98\x80",0,  1, 1, 5, DC,DC,DC,DC},
{"utf8 dot=1 cp",       "^.$",             "\xf0\x9f\x98\x80",0,   1, 0, 4, DC,DC,DC,DC},
{"utf8 class range",    "[\xc3\xa0-\xc3\xbf]","z\xc3\xa9",0,       1, 1, 3, DC,DC,DC,DC},
{"group + capture",     "a(bc)d",          "xabcd",      0,        1, 1, 5, 2, 4, DC,DC},
{"two captures",        "(a)(b)",          "zab",        0,        1, 1, 3, 1, 2, 2, 3},
{"nested captures",     "((a)b)",          "ab",         0,        1, 0, 2, 0, 2, 0, 1},
{"non-capture group",   "(?:ab)+",         "abab",       0,        1, 0, 4, -1,-1,DC,DC},
{"unset group",         "(a)|(b)",         "b",          0,        1, 0, 1, -1,-1, 0, 1},
{"alternation",         "cat|dog",         "hotdog",     0,        1, 3, 6, DC,DC,DC,DC},
{"alt leftmost-first",  "a|ab",            "ab",         0,        1, 0, 1, DC,DC,DC,DC},
{"alt in group",        "x(a|bb)y",        "xbby",       0,        1, 0, 4, 1, 3, DC,DC},
{"alt empty branch",    "^(a|)$",          "",           0,        1, 0, 0, 0, 0, DC,DC},
{"quant *",             "ab*c",            "ac",         0,        1, 0, 2, DC,DC,DC,DC},
{"quant * greedy",      "a*",              "aaa",        0,        1, 0, 3, DC,DC,DC,DC},
{"quant +",             "ab+c",            "abbbc",      0,        1, 0, 5, DC,DC,DC,DC},
{"quant + neg",         "ab+c",            "ac",         0,        0, DC,DC,DC,DC,DC,DC},
{"quant ?",             "ab?c",            "ac",         0,        1, 0, 2, DC,DC,DC,DC},
{"quant {n}",           "a{3}",            "aaaa",       0,        1, 0, 3, DC,DC,DC,DC},
{"quant {n} neg",       "^a{3}$",          "aa",         0,        0, DC,DC,DC,DC,DC,DC},
{"quant {n,}",          "^a{2,}$",         "aaaa",       0,        1, 0, 4, DC,DC,DC,DC},
{"quant {n,m}",         "a{2,3}",          "aaaaa",      0,        1, 0, 3, DC,DC,DC,DC},
{"quant {,m}",          "^a{,2}",          "aaa",        0,        1, 0, 2, DC,DC,DC,DC},
{"quant on group",      "(ab){2}",         "xababy",     0,        1, 1, 5, 3, 5, DC,DC},
{"quant on group {n,m}","(ab){1,2}",       "ababab",     0,        1, 0, 4, 2, 4, DC,DC},
{"lazy *?",             "a.*?c",           "abcbc",      0,        1, 0, 3, DC,DC,DC,DC},
{"lazy +?",             "a.+?c",           "abcbc",      0,        1, 0, 3, DC,DC,DC,DC},
{"lazy ??",             "^ab??",           "ab",         0,        1, 0, 1, DC,DC,DC,DC},
{"lazy {n,m}?",         "^a{1,3}?",        "aaa",        0,        1, 0, 1, DC,DC,DC,DC},
{"lazy group",          "(a|ab)*?c",       "abc",        0,        1, 0, 3, DC,DC,DC,DC},
{"icase literal",       "ABC",             "xabc",       RE_ICASE, 1, 1, 4, DC,DC,DC,DC},
{"icase off",           "ABC",             "xabc",       0,        0, DC,DC,DC,DC,DC,DC},
{"icase class",         "[a-f]+",          "XBEAD",      RE_ICASE, 1, 1, 5, DC,DC,DC,DC},
{"icase neg class",     "[^a-f]+",         "BEADXY",     RE_ICASE, 1, 4, 6, DC,DC,DC,DC},
{"empty-loop guard",    "^(a*)*$",         "aaa",        0,        1, 0, 3, DC,DC,DC,DC},
{"error: bad quant",    "a{2,1}",          "aa",         0,       -1, DC,DC,DC,DC,DC,DC},
{"error: unclosed (",   "(ab",             "ab",         0,       -1, DC,DC,DC,DC,DC,DC},
{"error: unclosed [",   "[ab",             "ab",         0,       -1, DC,DC,DC,DC,DC,DC},
{"error: trailing \\",  "ab\\",            "ab",         0,       -1, DC,DC,DC,DC,DC,DC},
{"error: backref",      "(a)\\1",          "aa",         0,       -1, DC,DC,DC,DC,DC,DC},
{"error: lookahead",    "(?=a)",           "a",          0,       -1, DC,DC,DC,DC,DC,DC},
{"error: bare *",       "*a",              "a",          0,       -1, DC,DC,DC,DC,DC,DC},
{"step budget RE_LIMIT","^(a+)+$",         "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaab", 0, -2, DC,DC,DC,DC,DC,DC},
};

static int chk(const char *what, int got, int want)
{
    if (want == DC || got == want) return 0;
    printf("      MISMATCH %s: got %d want %d\n", what, got, want);
    return 1;
}

int main(void)
{
    int i, n = (int)(sizeof rows / sizeof rows[0]), bad = 0, basediff = 0;
    for (i = 0; i < n; i++) {
        const struct row *t = &rows[i];
        void *ws[512];
        int c[8], j, r, f = 0;
        for (j = 0; j < 8; j++) c[j] = -9;
        r = re_match(t->pat, t->text, t->fl, c, 4, ws, (unsigned)sizeof ws);
        f |= chk("ret", r, t->r);
        if (r == RE_MATCH) {
            f |= chk("start", c[0], t->s); f |= chk("end", c[1], t->e);
            f |= chk("g1.s", c[2], t->g1s); f |= chk("g1.e", c[3], t->g1e);
            f |= chk("g2.s", c[4], t->g2s); f |= chk("g2.e", c[5], t->g2e);
        }
        printf("%-3d %-22s %-18s -> %s\n", i, t->feat, t->pat, f ? "**FAIL**" : "ok");
        bad += !!f;
    }
    printf("\nfeature rows=%d ok=%d fail=%d\n", n, n - bad, bad);
    return bad != 0;
}
