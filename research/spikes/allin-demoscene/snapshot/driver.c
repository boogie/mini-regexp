/* driver.c -- CLI for difftest.py (NOT part of the engine size).
 * usage: ./driver [-i] PATTERN TEXT
 * stdout: "match START END" | "nomatch" | "error N", then "K START END" per capture group. */
#include <stdio.h>
#include <string.h>
#include "re.h"

#define NCAPS 10

/* Number of capturing groups in the pattern (skips escapes and classes). */
static int count_groups(const char *p)
{
    int n = 0;
    for (; *p; p++) {
        if (*p == '\\' && p[1]) p++;
        else if (*p == '[') { if (p[1] == '^') p++; if (p[1] == ']') p++; while (p[1] && p[1] != ']') { if (p[1] == '\\' && p[2]) p++; p++; } if (p[1]) p++; }
        else if (*p == '(' && p[1] != '?') n++;
    }
    return n;
}

int main(int argc, char **argv)
{
    static void *ws[8 * 65536];        /* room for 64K group iterations on the match path */
    int caps[2 * NCAPS], i, r, n, a = 1;
    unsigned flags = 0;
    if (argc > 1 && !strcmp(argv[1], "-i")) { flags = RE_ICASE; a = 2; }
    if (argc < a + 2) { puts("error usage"); return 2; }
    r = re_match(argv[a], argv[a + 1], flags, caps, NCAPS, ws, (unsigned)sizeof ws);
    if (r < 0) { printf("error %d\n", r); return 1; }
    if (r == 0) { puts("nomatch"); return 0; }
    printf("match %d %d\n", caps[0], caps[1]);
    n = count_groups(argv[a]);
    for (i = 1; i <= n && i < NCAPS; i++) printf("%d %d %d\n", i, caps[2 * i], caps[2 * i + 1]);
    return 0;
}
