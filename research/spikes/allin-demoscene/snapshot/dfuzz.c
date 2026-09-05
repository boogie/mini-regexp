/* Differential fuzz: merged engine vs the untouched baseline, in one binary.
 * Compares the return code AND the whole capture vector for every case. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int re_match(const char *pat, const char *text, unsigned flags, int *caps, int ncaps);
int base_re_match(const char *pat, const char *text, unsigned flags, int *caps, int ncaps);

static unsigned long st_;
static unsigned rnd(unsigned n) { st_ = st_ * 6364136223846793005ULL + 1442695040888963407ULL;
                                  return (unsigned)((st_ >> 33) % n); }

/* token alphabet: everything the engine can trip over */
static const char *tok[] = {
 "a","b","c","z","A","B","0","9","_"," ","\n","\t","é","本","😀","\xc3","\x80","\xff",
 ".","^","$","*","+","?","|","(",")","(?:","[","]","-","{","}","\\","\\d","\\D","\\w","\\W",
 "\\s","\\S","\\b","\\B","\\n","\\t","\\r","\\.","\\\\","\\-","\\]","\\q","\\1",
 "[a-c]","[^a-c]","[\\d-]","[]]","[^]]","[a-\\d]","[z-a]","{2}","{1,3}","{,2}","{2,}","{3,1}",
 "{}","{,}","*?","+?","??","{1,2}?","(a)","(a|b)","(?:ab)","(a*)","a**","(?i)","(?=a)"
};
#define NTOK (int)(sizeof tok / sizeof tok[0])
static const char *txts[] = {
 "","a","ab","abc","aaa","aab","aaaa","abab","ABC","Ab","0a9","_a_"," a b","a\nb","x","zz",
 "éé","Éé","本日","😀a","\xc3\x28","\xff\xfe","aaaaaaaaaa","abcabcabc","the quick brown","a-b",
 "]a[","{}","a{2}","\\","aA","  ","\t\n\r"
};
#define NTXT (int)(sizeof txts / sizeof txts[0])

int main(int argc, char **argv)
{
    long total = argc > 1 ? atol(argv[1]) : 200000;
    long i, diffs = 0;
    char pat[512];
    st_ = argc > 2 ? strtoul(argv[2], 0, 10) : 1;
    for (i = 0; i < total; i++) {
        int nt = 1 + (int)rnd(7), j, na, nb, k, nc = (int)rnd(6);
        unsigned fl = rnd(2) ? 1u : 0u;   /* RE_ICASE == 1 */
        const char *txt = txts[rnd(NTXT)];
        int ca[24], cb[24];
        pat[0] = 0;
        for (j = 0; j < nt; j++) strcat(pat, tok[rnd(NTOK)]);
        for (k = 0; k < 24; k++) ca[k] = cb[k] = -77;
        na = re_match(pat, txt, fl, ca, nc);
        nb = base_re_match(pat, txt, fl, cb, nc);
        if (na != nb || memcmp(ca, cb, sizeof ca)) {
            if (++diffs <= 10)
                printf("DIFF pat=<%s> txt=<%s> fl=%u nc=%d new=%d base=%d\n"
                       "   new caps: %d %d %d %d %d %d\n   base caps: %d %d %d %d %d %d\n",
                       pat, txt, fl, nc, na, nb,
                       ca[0],ca[1],ca[2],ca[3],ca[4],ca[5], cb[0],cb[1],cb[2],cb[3],cb[4],cb[5]);
        }
    }
    printf("cases=%ld diffs=%ld\n", total, diffs);
    return diffs != 0;
}
