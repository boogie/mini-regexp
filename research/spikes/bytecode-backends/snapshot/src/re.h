#ifndef MINI_REGEXP_H
#define MINI_REGEXP_H

enum { RE_SPACE = -2, RE_BUDGET = -1, RE_NOMATCH = 0, RE_MATCH = 1 };

#define RE_CAPTURE_SLOTS(code) ((unsigned)(code)[0])
#define RE_WORKSPACE_SLOTS(code, depth) ((RE_CAPTURE_SLOTS(code) + 2u) * (unsigned)(depth))

/* code must be trusted compiler output. The caller provides RE_CAPTURE_SLOTS(code)
 * capture pointers and RE_WORKSPACE_SLOTS(code, depth) const-void-pointer workspace
 * slots. Pairs 0, 1, ... are the overall match and capture groups. Exhausting steps
 * returns RE_BUDGET; exhausting workspace_depth returns RE_SPACE. */
int re_match(const unsigned char *code, const char *text, const char **captures,
             const void **workspace, unsigned steps, unsigned workspace_depth);

#endif
