/* difftest driver for SLRE (cesanta/slre).  CLI: ./driver [-i] PATTERN TEXT
 * slre_match() returns only the END offset of the leftmost match (baz() adds the
 * start index to the length and drops it), so the start is not recoverable from
 * the API.  We replicate slre's own scan loop (baz(): try doh() at every start
 * offset in order) here: prefix the pattern with '^' (in slre '^' means "offset 0
 * of the buffer passed in") and call slre_match(text+p) for p = 0..len; the first
 * p that matches is the start.  If the user's pattern already begins with '^',
 * slre itself only tries p = 0, so we do the same.  Captures come from slre_cap. */
#include <stdio.h>
#include <string.h>
#include "src/slre.h"
#define MAXCAPS 100
int main(int argc, char **argv) {
  int flags = 0, a = 1, plen, tlen, pmax, p, i, ng = 0;
  const char *pat, *text;
  static char apat[8192];
  struct slre_cap caps[MAXCAPS];
  if (a < argc && strcmp(argv[a], "-i") == 0) { flags = SLRE_IGNORE_CASE; a++; }
  if (argc - a != 2) { fprintf(stderr, "usage: driver [-i] PATTERN TEXT\n"); return 2; }
  pat = argv[a]; text = argv[a + 1];
  plen = (int) strlen(pat); tlen = (int) strlen(text);
  if (plen + 2 > (int) sizeof apat) { puts("error"); return 0; }
  /* a leading quantifier is an slre error; the '^' prefix would mask it */
  if (pat[0] == '*' || pat[0] == '+' || pat[0] == '?') { puts("error"); return 0; }
  apat[0] = '^'; memcpy(apat + 1, pat, (size_t) plen + 1);
  /* count '(' the way slre's foo() does (skip escapes and [...] sets) */
  for (i = 0; i < plen;) {
    if (pat[i] == '\\') i += pat[i + 1] == 'x' ? 4 : 2;
    else if (pat[i] == '[') { i++; while (i < plen && pat[i] != ']') i += pat[i] == '\\' ? (pat[i + 1] == 'x' ? 4 : 2) : 1; i++; }
    else { if (pat[i] == '(') ng++; i++; }
  }
  if (ng > MAXCAPS) ng = MAXCAPS;
  pmax = pat[0] == '^' ? 0 : tlen;
  for (p = 0; p <= pmax; p++) {
    int r;
    memset(caps, 0, sizeof caps);
    r = slre_match(apat, text + p, tlen - p, caps, MAXCAPS, flags);
    if (r >= 0) {
      printf("match %d %d\n", p, p + r);
      for (i = 0; i < ng; i++) {
        if (caps[i].ptr) printf("%d %d %d\n", i + 1, (int) (caps[i].ptr - text), (int) (caps[i].ptr - text) + caps[i].len);
        else printf("%d -1 -1\n", i + 1);
      }
      return 0;
    }
    if (r != SLRE_NO_MATCH) { printf("error %d\n", r); return 0; }
  }
  puts("nomatch");
  return 0;
}
