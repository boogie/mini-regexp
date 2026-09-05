/* Host driver for Henry Spencer's regexp(3) (garyhouston/regexp.old) implementing the
 * PROTOCOL.md CLI: ./engine_cli [-i] PATTERN TEXT
 * Adaptations: regexec() already searches anywhere (unanchored scan) and fills
 * startp[]/endp[] for groups 0..9, so no wrapping is needed.  The package has no
 * case-insensitive mode: -i is accepted and IGNORED (counted as a missing feature).
 * regerror() is replaced by a non-exiting stub so compile errors print "error". */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "regexp.h"

static int reg_failed;
void regerror(char *s) { reg_failed = 1; fprintf(stderr, "%s\n", s); }

int main(int argc, char **argv)
{
    int a = 1;
    if (a < argc && strcmp(argv[a], "-i") == 0) a++;      /* no -i support: ignored */
    if (argc - a != 2) { puts("error"); return 2; }
    const char *pat = argv[a], *text = argv[a + 1];
    regexp *r = regcomp(pat);
    if (r == NULL || reg_failed) { puts("error"); return 1; }
    if (!regexec(r, text) || reg_failed) { puts("nomatch"); return 0; }
    printf("match %ld %ld\n", (long)(r->startp[0] - text), (long)(r->endp[0] - text));
    for (int g = 1; g < NSUBEXP; g++) {
        if (r->startp[g] && r->endp[g])
            printf("%d %ld %ld\n", g, (long)(r->startp[g] - text), (long)(r->endp[g] - text));
        else
            printf("%d -1 -1\n", g);
    }
    free(r);
    return 0;
}
