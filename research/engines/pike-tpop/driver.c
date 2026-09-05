/* Host driver for the PROTOCOL.md CLI contract:  ./driver [-i] PATTERN TEXT
 * Adaptations (the engine's API is a bare boolean, no positions, no flags):
 *  - START: scan start offsets with matchhere() (exactly what match() does internally).
 *  - END:   the engine cannot report it; derived by probing matchhere(P"$", text[0..e))
 *           for each e (the appended '$' pins the match end to e). Default = smallest e
 *           (the "leftmost, shortest" match the engine itself finds); PIKE_LONGEST=1 in
 *           the environment = largest e (leftmost-longest). A trailing '$' in P already
 *           pins e = len.
 *  - -i:    emulated by ASCII-lowercasing pattern and text (engine has no flags).
 *  - Captures: impossible; only line 1 is printed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "pike.h"

static void lower(char *s) { for (; *s; s++) *s = (char)tolower((unsigned char)*s); }

int main(int argc, char **argv)
{
    int ci = argc > 1 && strcmp(argv[1], "-i") == 0, ai = ci ? 2 : 1;
    if (argc - ai != 2) { puts("error"); return 2; }
    char *pat = strdup(argv[ai]), *text = strdup(argv[ai + 1]);
    if (ci) { lower(pat); lower(text); }
    size_t n = strlen(text), s, e, plen;
    int anchored = pat[0] == '^', longest = getenv("PIKE_LONGEST") != NULL;
    char *p = anchored ? pat + 1 : pat, *buf = malloc(n + 1), *p2 = malloc(strlen(p) + 2);
    plen = strlen(p);
    strcpy(p2, p); if (!(plen && p[plen - 1] == '$')) strcat(p2, "$");  /* pin the end */
    for (s = 0; s <= n; s++) {
        if (!matchhere(p, text + s)) { if (anchored) break; continue; }
        for (e = longest ? n : s; ; longest ? e-- : e++) {
            memcpy(buf, text, e); buf[e] = 0;
            if (matchhere(p2, buf + s)) { printf("match %zu %zu\n", s, e); return 0; }
            if (longest ? e == s : e == n) break;
        }
        puts("error"); return 1;   /* unreachable: some prefix must end the match */
    }
    puts("nomatch");
    return 0;
}
