#ifndef COMMAND_MATCHER_H
#define COMMAND_MATCHER_H

#include "../../src/re.h"
#include "command_re.h"

enum { COMMAND_RE_WORKSPACE_DEPTH = 64 };

struct command_matcher {
    const char *captures[command_re_capture_slots];
    const void *workspace[(command_re_capture_slots + 2u) * COMMAND_RE_WORKSPACE_DEPTH];
};

int command_match(struct command_matcher *matcher, const char *text);

#endif
