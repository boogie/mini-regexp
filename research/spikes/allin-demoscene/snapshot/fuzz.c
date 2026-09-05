/* fuzz.c -- random pattern/text loop for ASan/UBSan (NOT part of the engine).
 * usage: ./fuzz SECONDS [SEED]
 * Buffers are exactly sized heap allocations so any read past the NUL is caught. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "re.h"

static const char alphabet[] = "aab()[]^$|*+?{},-\\.dwsDWSbBntr019_ \n\xc3\xa9\xe6\x97\xa5\xf0\x9f\x98\x80\x80\xff\xc3";

static unsigned rnd(unsigned *x) { *x = *x * 1103515245u + 12345u; return *x >> 8; }

static char *gen(unsigned *x, int maxlen, int structured)
{
    int n = rnd(x) % (maxlen + 1), i;
    char *b = malloc(n + 1);
    for (i = 0; i < n; i++) {
        if (structured || rnd(x) % 4) b[i] = alphabet[rnd(x) % (sizeof alphabet - 1)];
        else b[i] = 1 + rnd(x) % 255;              /* any non-NUL byte */
    }
    b[n] = 0;
    return b;
}

int main(int argc, char **argv)
{
    int secs = argc > 1 ? atoi(argv[1]) : 60;
    unsigned x = argc > 2 ? (unsigned)atoi(argv[2]) : 12345u;
    long cases = 0, results[4] = {0, 0, 0, 0};
    time_t t0 = time(0);
    while (time(0) - t0 < secs) {
        char *pat = gen(&x, 24, 1), *text = gen(&x, 20, 0);
        int caps[20], i, r, n = (int)strlen(text);
        unsigned flags = rnd(&x) & 3;
        /* An exactly sized heap workspace of a random size: ASan catches any step outside
         * it, and the small sizes exercise the "workspace ran out" path. */
        unsigned wsbytes = (rnd(&x) % 129u) * (unsigned)sizeof(void *);
        void *ws = malloc(wsbytes ? wsbytes : 1);
        memset(caps, 0x55, sizeof caps);
        r = re_match(pat, text, flags, caps, 10, ws, wsbytes);
        free(ws);
        if (r < -2 || r > 1) { printf("BAD RETURN %d pat=%s\n", r, pat); return 1; }
        results[r + 2]++;
        if (r == 1) {
            if (caps[0] < 0 || caps[0] > caps[1] || caps[1] > n) { printf("BAD SPAN %d %d\n", caps[0], caps[1]); return 1; }
            for (i = 1; i < 10; i++)
                if (!(caps[2 * i] == -1 && caps[2 * i + 1] == -1) &&
                    (caps[2 * i] < 0 || caps[2 * i] > caps[2 * i + 1] || caps[2 * i + 1] > n)) {
                    printf("BAD GROUP %d: %d %d\n", i, caps[2 * i], caps[2 * i + 1]); return 1;
                }
        }
        free(pat); free(text);
        cases++;
    }
    printf("cases=%ld limit=%ld error=%ld nomatch=%ld match=%ld crashes=none\n",
           cases, results[0], results[1], results[2], results[3]);
    return 0;
}
