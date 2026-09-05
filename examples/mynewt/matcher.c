#include "matcher.h"

int command_match(struct command_matcher *matcher, const char *text)
{
    return re_match(command_re, text, matcher->captures, matcher->workspace, 10000,
                    COMMAND_RE_WORKSPACE_DEPTH);
}
