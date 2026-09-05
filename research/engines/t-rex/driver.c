/* difftest driver for T-Rex 1.3 (trex.c) — CLI contract from PROTOCOL.md:  ./engine_cli [-i] PATTERN TEXT */
#include <stdio.h>
#include <string.h>
#include "trex.h"
int main(int argc, char **argv)
{
    int a = 1;
    if (a < argc && !strcmp(argv[a], "-i")) a++;   /* -i is IGNORED: T-Rex has no case-insensitive mode at all */
    if (argc - a < 2) { puts("error usage"); return 2; }
    const char *pat = argv[a], *text = argv[a + 1], *err = NULL, *b, *e;
    /* guard: trex_compile("") does malloc(0) + realloc(p,0) and then writes node 0 -> heap corruption (trex.c:536-541,91-96) */
    if (!*pat) { puts("error empty-pattern-guard"); return 1; }
    TRex *x = trex_compile(pat, &err);
    if (!x) { printf("error %s\n", err ? err : "?"); return 1; }
    /* trex_search = search-anywhere API (scans start positions itself); captures via trex_getsubexp (index 0 = whole match) */
    if (!trex_search(x, text, &b, &e)) { puts("nomatch"); trex_free(x); return 0; }
    printf("match %ld %ld\n", (long)(b - text), (long)(e - text));
    int n = trex_getsubexpcount(x);
    for (int i = 1; i < n; i++) {
        TRexMatch m;
        trex_getsubexp(x, i, &m);
        if (m.begin) printf("%d %ld %ld\n", i, (long)(m.begin - text), (long)(m.begin + m.len - text));
        else         printf("%d -1 -1\n", i);   /* begin==NULL: group never entered / cleared on failure */
    }
    trex_free(x);
    return 0;
}
