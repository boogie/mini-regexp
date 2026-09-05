/* difftest driver for tiny-regex-c -- CLI contract from PROTOCOL.md:  ./engine_cli [-i] PATTERN TEXT
 * re_matchp() already searches anywhere (returns start index, *len = match length), so no wrapping
 * or start-position scanning is needed.  The engine has no capture groups -> no "N START END" lines.
 * It has no case-insensitive mode -> "-i" is accepted and IGNORED (such cases can only pass by luck). */
#include <stdio.h>
#include <string.h>
#include "re.h"

int main(int argc, char **argv)
{
    int a = 1, len = 0, idx;
    re_t re;
    if (a < argc && strcmp(argv[a], "-i") == 0) a++;
    if (argc - a != 2) { puts("error"); return 2; }
    re = re_compile(argv[a]);
    if (!re) { puts("error"); return 1; }           /* re_compile returns NULL on malformed patterns */
    idx = re_matchp(re, argv[a + 1], &len);
    if (idx < 0) puts("nomatch");
    else printf("match %d %d\n", idx, idx + len);   /* byte offsets, half-open */
    return 0;
}
