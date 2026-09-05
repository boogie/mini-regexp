#include <assert.h>
#include <stddef.h>

#include "../../../src/re.h"

static const unsigned char capture_code[] = {
    6,7,27,0,10,0,7,11,0,10,2,6,1,97,97,10,3,8,8,0,10,4,6,1,98,98,10,5,
    10,1,0,6,128,8,221,255
};
static const unsigned char budget_code[] = {
    4,7,49,0,10,0,1,10,2,6,1,97,97,7,7,0,6,1,97,97,8,246,255,10,3,7,21,0,
    10,2,6,1,97,97,7,7,0,6,1,97,97,8,246,255,10,3,8,232,255,2,10,1,0,6,
    128,8,199,255
};
static const unsigned char dot_code[] = {
    2,7,9,0,10,0,6,129,10,10,10,1,0,6,128,8,239,255
};

int main(void)
{
    const char *captures[6];
    const void *workspace[RE_WORKSPACE_SLOTS(capture_code, 64)];
    const char matched[] = "b";
    const char truncated[] = {(char)0xf0, '\0'};

    assert(re_match(capture_code, matched, captures, workspace, 1000, 64) == 1);
    assert(captures[0] == matched && captures[1] == matched + 1);
    assert(captures[2] == NULL && captures[3] == NULL);
    assert(captures[4] == matched && captures[5] == matched + 1);
    assert(re_match(budget_code, "aaaaaaaaaaaaaaaaaaaaaaaaaaab", captures, workspace, 20, 64) == RE_BUDGET);
    assert(re_match(budget_code, "aaaaaaaaaaaaaaaaaaaaaaaaaaab", captures, workspace, 1000000, 1) == RE_SPACE);
    assert(re_match(dot_code, truncated, captures, workspace, 100, 64) == 1);
    assert(captures[0] == truncated && captures[1] == truncated + 1);
    return 0;
}
