/* stackprobe.c -- peak stack consumption of re_match vs. text length,
 * plus the exact frame composition at the peak (for Cortex-M4 projection). */
#include <stdio.h>
#include <string.h>
#include "re.h"

extern char *g_stack_hi, *g_stack_lo;
extern unsigned long g_calls;
extern void *g_peak_stack[]; extern int g_peak_depth, g_depth;


static const char *pats[] = { "a*", ".*", "(a)*", "(a|b)*", "(\\w+ )*" };
static const int lens[] = { 8, 32, 128, 256 };

__attribute__((no_instrument_function))
static void mktext(char *b, int n, const char *pat)
{
    int i;
    if (!strcmp(pat, "(\\w+ )*")) { for (i = 0; i < n; i++) b[i] = (i % 4 == 3) ? ' ' : 'a'; }
    else if (!strcmp(pat, "(a|b)*")) { for (i = 0; i < n; i++) b[i] = (i & 1) ? 'b' : 'a'; }
    else { for (i = 0; i < n; i++) b[i] = 'a'; }
    b[n] = 0;
}

/* distinct function pointers seen at the peak, with counts */
__attribute__((no_instrument_function))
static void dump_peak(void)
{
    void *fn[32]; int cnt[32], nf = 0, i, j;
    for (i = 0; i < g_peak_depth; i++) {
        for (j = 0; j < nf; j++) if (fn[j] == g_peak_stack[i]) break;
        if (j == nf) { if (nf == 32) continue; fn[nf] = g_peak_stack[i]; cnt[nf] = 0; nf++; }
        cnt[j]++;
    }
    printf("      peak_depth=%d frames:", g_peak_depth);
    for (j = 0; j < nf; j++) printf("  %p x%d", fn[j], cnt[j]);
    printf("\n      re_match@%p\n", (void *)(size_t)re_match);
}

__attribute__((no_instrument_function))
int main(void)
{
    static char text[512];
    int caps[20], p, l, r;
    static void *ws[4096];
    char anchor;
    long peak[5][4];

    for (p = 0; p < 5; p++) {
        for (l = 0; l < 4; l++) {
            mktext(text, lens[l], pats[p]);
            g_stack_hi = &anchor; g_stack_lo = &anchor; g_calls = 0;
            g_depth = 0; g_peak_depth = 0;
            r = re_match(pats[p], text, 0, caps, 10, ws, (unsigned)sizeof ws);
            peak[p][l] = (long)(g_stack_hi - g_stack_lo);
            printf("%-10s len=%-4d peak=%-7ld calls=%-7lu r=%d\n",
                   pats[p], lens[l], peak[p][l], g_calls, r);
            dump_peak();
        }
        printf("  -> %-10s 8->256: %ld B over 248 chars = %.3f B/char  %s\n\n",
               pats[p], peak[p][3] - peak[p][0],
               (double)(peak[p][3] - peak[p][0]) / 248.0,
               (peak[p][3] == peak[p][0]) ? "[CONSTANT]" : "[GROWS]");
    }
    return 0;
}
