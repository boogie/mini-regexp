/* driver.c - PROTOCOL.md CLI ("./driver [-i] PATTERN TEXT") for Plan 9 / plan9port libregexp (regcomp9/regexec9).
 * Host build: see BUILD.txt.  Adaptations (the engine has none of these): -i is emulated by ASCII-lowercasing
 * pattern and text in the driver (byte lengths unchanged, so offsets stay valid); regerror9 is overridden here
 * because the library's default regerror.c calls exit() without a reportable status. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "regexp9.h"

#define NGRP 32                       /* Resub slots handed to regexec9 (slot 0 = whole match) */

void regerror9(char *s) { (void)s; puts("error"); exit(0); }   /* library error hook (overridable by design) */

static void lower(char *s) { for (; *s; s++) if (*s >= 'A' && *s <= 'Z') *s += 'a' - 'A'; }

int main(int argc, char **argv)
{
    int ci = 0, n;
    char *pat, *text;
    Resub m[NGRP];
    Reprog *p;

    if (argc > 1 && strcmp(argv[1], "-i") == 0) { ci = 1; argv++; argc--; }
    if (argc != 3) { puts("error"); return 2; }
    pat = argv[1]; text = argv[2];
    if (ci) { lower(pat); lower(text); }
    p = regcomp9(pat);                                 /* NULL => regerror9 already printed "error" */
    if (!p) { puts("error"); return 0; }
    memset(m, 0, sizeof m);                            /* sp/ep == 0 means "use whole string" / "unset" */
    n = regexec9(p, text, m, NGRP);                    /* search-anywhere, leftmost(-longest) is native */
    if (n < 0) { puts("error"); return 0; }            /* ran out of thread-list space even after malloc fallback */
    if (n == 0) { puts("nomatch"); return 0; }
    printf("match %ld %ld\n", (long)(m[0].s.sp - text), (long)(m[0].e.ep - text));
    for (n = 1; n < NGRP; n++)
        if (m[n].s.sp) printf("%d %ld %ld\n", n, (long)(m[n].s.sp - text), (long)(m[n].e.ep - text));
        else printf("%d -1 -1\n", n);
    return 0;
}
