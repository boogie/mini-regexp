/* difftest driver for re1.5 (PROTOCOL.md CLI contract).  Build:
 *   cc -O1 -w -Isrc    -o re15_cli        driver.c mp_all.c              (upstream pfalcon/re1.5)
 *   cc -O1 -w -Imp_lib -DMPFORK -o re15_cli_fork driver.c mpfork_all.c  (MicroPython lib/re1.5 fork)
 * Adaptations: re1.5 has no case-insensitive flag -> "-i" is accepted and IGNORED (those cases fail
 * as "missing feature").  Search-anywhere is native (is_anchored=0 uses the built-in RSplit/Any/Jmp
 * prefix).  Captures are native (Save slots 2n/2n+1).  Bytecode buffer is malloc'd by the driver. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "re1.5.h"
#define MAXCAP 64
int main(int argc, char **argv) {
    int ai = 1;
    if (ai < argc && !strcmp(argv[ai], "-i")) ai++;            /* ignored: no such feature */
    if (argc - ai != 2) { puts("error"); return 2; }
    const char *pat = argv[ai], *text = argv[ai + 1];
    int size = re1_5_sizecode(pat);
    if (size < 0) { puts("error"); return 1; }
    ByteProg *prog = malloc(sizeof(ByteProg) + size);
    if (re1_5_compilecode(prog, pat) != 0) { puts("error"); return 1; }
#ifdef MPFORK
    Subject subj = { text, text, text + strlen(text) };       /* begin_line, begin, end */
#else
    Subject subj = { text, text + strlen(text) };
#endif
    const char *caps[MAXCAP] = { 0 };
    int ncap = 2 * (prog->sub + 1);
    if (ncap > MAXCAP) ncap = MAXCAP;
    if (!re1_5_recursiveloopprog(prog, &subj, caps, ncap, 0)) { puts("nomatch"); return 0; }
    printf("match %ld %ld\n", (long)(caps[0] - text), (long)(caps[1] - text));
    for (int g = 1; g < ncap / 2; g++) {
        if (caps[2 * g] && caps[2 * g + 1])
            printf("%d %ld %ld\n", g, (long)(caps[2 * g] - text), (long)(caps[2 * g + 1] - text));
        else
            printf("%d -1 -1\n", g);
    }
    return 0;
}
