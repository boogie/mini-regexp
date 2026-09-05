/* Host driver for musl regex (TRE): CLI contract from PROTOCOL.md.
   ./driver [-i] PATTERN TEXT  ->  "match S E" | "nomatch" | "error", then "N S E" per group.
   POSIX regexec already searches anywhere and returns captures, so no wrapping is needed.
   Flags: REG_EXTENDED always (ERE), REG_ICASE for -i, REG_NEWLINE if env MUSL_REG_NEWLINE is set.
   Locale: LC_CTYPE=en_US.UTF-8 so the host mbtowc/iswctype give UTF-8 + Unicode class semantics
   (the same thing musl's own libc would do in its C.UTF-8 locale). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <regex.h>            /* -> hostinc/regex.h stub (musl layout) */

int main(int argc, char **argv)
{
    int icase = 0, cflags = REG_EXTENDED, a = 1;
    regex_t re;
    regmatch_t m[64];
    size_t n, i;
    if (a < argc && strcmp(argv[a], "-i") == 0) { icase = 1; a++; }
    if (argc - a != 2) { fprintf(stderr, "usage: %s [-i] PATTERN TEXT\n", argv[0]); return 2; }
    if (!setlocale(LC_CTYPE, "en_US.UTF-8")) setlocale(LC_CTYPE, "C.UTF-8");
    if (icase) cflags |= REG_ICASE;
    if (getenv("MUSL_REG_NEWLINE")) cflags |= REG_NEWLINE;
    if (regcomp(&re, argv[a], cflags) != 0) { puts("error"); return 0; }
    n = re.re_nsub + 1;
    if (n > 64) n = 64;
    if (regexec(&re, argv[a + 1], n, m, 0) != 0) { puts("nomatch"); regfree(&re); return 0; }
    printf("match %d %d\n", (int)m[0].rm_so, (int)m[0].rm_eo);
    for (i = 1; i < n; i++) printf("%zu %d %d\n", i, (int)m[i].rm_so, (int)m[i].rm_eo);
    regfree(&re);
    return 0;
}
