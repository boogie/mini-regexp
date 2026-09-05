/* Host driver for subreg implementing the PROTOCOL.md CLI: ./engine_cli [-i] PATTERN TEXT
 * subreg only does anchored full-string matches (^ and $ implied, no search API), so we adapt:
 *  - wrap:  [(?i)](?:PAT)(.*)   -> the last capture (.*) tells us where PAT stopped (match end)
 *  - scan start positions 0..len (leftmost); a leading '^' restricts to start 0
 *  - a trailing unescaped '$' drops the (.*) so PAT must reach end of text
 * Captures come back in subreg's own order (completion order, one per repetition); reported as-is. */
#include <stdio.h>
#include <string.h>
#include "subreg.h"
#define MAXCAP 64
int main(int argc, char **argv) {
    int ci = 0, a = 1, anch_start = 0, anch_end = 0, n, g, ng;
    size_t plen, tlen, s, k, bs = 0;
    const char *pat, *text; char wrapped[8192]; subreg_capture_t caps[MAXCAP];
    if (a < argc && !strcmp(argv[a], "-i")) { ci = 1; a++; }
    if (argc - a < 2) { puts("error"); return 2; }
    pat = argv[a]; text = argv[a + 1]; plen = strlen(pat); tlen = strlen(text);
    if (plen && pat[0] == '^') { anch_start = 1; pat++; plen--; }
    for (k = plen; k >= 2 && pat[k - 2] == '\\'; k--) bs++;           /* backslashes before last char */
    if (plen && pat[plen - 1] == '$' && (bs % 2) == 0) { anch_end = 1; plen--; }
    if (plen + 16 >= sizeof wrapped) { puts("error"); return 2; }
    snprintf(wrapped, sizeof wrapped, "%s(?:%.*s)%s", ci ? "(?i)" : "", (int)plen, pat, anch_end ? "" : "(.*)");
    for (s = 0; s <= tlen; s++) {
        n = subreg_match(wrapped, text + s, caps, MAXCAP, 64);
        if (n < 0) { printf("error %d\n", n); return 1; }
        if (n > 0) {
            printf("match %zu %zu\n", s, anch_end ? tlen : (size_t)(caps[n - 1].start - text));
            ng = anch_end ? n - 1 : n - 2;                              /* caps[0]=whole input, last=(.*) */
            for (g = 1; g <= ng; g++)
                printf("%d %ld %ld\n", g, (long)(caps[g].start - text), (long)(caps[g].start - text + caps[g].length));
            return 0;
        }
        if (anch_start) break;
    }
    puts("nomatch");
    return 0;
}
