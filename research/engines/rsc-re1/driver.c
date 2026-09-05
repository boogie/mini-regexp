/* Host driver for rsc/re1 (https://github.com/rsc/re1) implementing the PROTOCOL.md CLI contract:
 *   ./engine_cli [-i] PATTERN TEXT  ->  line 1 "match START END" | "nomatch" | "error"; then "N S E" per group.
 * Build (one binary per executor):
 *   cc -O1 -w -Isrc -DEXEC=pikevm            driver.c src/y.tab.c src/compile.c src/sub.c src/pike.c      -o re1_pike
 *   cc -O1 -w -Isrc -DEXEC=backtrack         driver.c src/y.tab.c src/compile.c src/sub.c src/backtrack.c -o re1_backtrack
 *   cc -O1 -w -Isrc -DEXEC=recursiveprog     driver.c src/y.tab.c src/compile.c src/recursive.c           -o re1_recursive
 *   cc -O1 -w -Isrc -DEXEC=recursiveloopprog driver.c src/y.tab.c src/compile.c src/recursive.c           -o re1_recursiveloop
 *   cc -O1 -w -Isrc -DEXEC=thompsonvm -DNOGROUPS driver.c src/y.tab.c src/compile.c src/thompson.c        -o re1_thompson
 * Adaptations: none needed for search/captures -- re1's parse() itself prepends a non-greedy ".*?" and wraps the
 * whole pattern in capture group 0, so sub[0..1] is the overall span and sub[2k..2k+1] is group k.
 * re1 has no case-insensitive mode at all, so "-i" is answered with "error" (counted as a missing feature).
 * re1 reports syntax errors / "backtrack overflow" via fatal() -> exit(2); the atexit hook maps that to "error". */
#include "regexp.h"
#ifndef EXEC
#define EXEC pikevm
#endif
static int printed;
static void onexit(void) { if (!printed) puts("error"); }
int main(int argc, char **argv)
{
	char *sub[MAXSUB], *text;
	Prog *prog;
	int i;
	atexit(onexit);
	if (argc > 1 && strcmp(argv[1], "-i") == 0) { puts("error"); printed = 1; return 0; }
	if (argc != 3) { puts("error"); printed = 1; return 2; }
	text = argv[2];
	prog = compile(parse(argv[1]));
	memset(sub, 0, sizeof sub);
	if (!EXEC(prog, text, sub, MAXSUB)) { puts("nomatch"); printed = 1; return 0; }
	printf("match %d %d\n", (int)(sub[0] - text), (int)(sub[1] - text));
#ifndef NOGROUPS
	for (i = 1; i < MAXSUB / 2; i++) {
		if (sub[2*i] && sub[2*i+1]) printf("%d %d %d\n", i, (int)(sub[2*i] - text), (int)(sub[2*i+1] - text));
		else printf("%d -1 -1\n", i);
	}
#endif
	printed = 1;
	return 0;
}
