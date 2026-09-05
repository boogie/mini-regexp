#include <assert.h>

#include "re_compile.h"

int main(void)
{
    unsigned char output[128];
    struct re_compile_scratch scratch;
    unsigned capacity;

    assert(re_compile(output, 0, "a", RE_COMPILE_SEARCH, &scratch) == RE_COMPILE_SPACE);
    for (capacity = 1; capacity < 18; capacity++)
        assert(re_compile(output, capacity, "a", RE_COMPILE_SEARCH, &scratch) == RE_COMPILE_SPACE);
    assert(re_compile(output, sizeof(output), "a", RE_COMPILE_SEARCH, &scratch) == 18);
    assert(re_compile(output, sizeof(output), "(", RE_COMPILE_SEARCH, &scratch) == RE_COMPILE_SYNTAX);
    assert(re_compile(output, sizeof(output), "(a*)*", RE_COMPILE_SEARCH, &scratch) == RE_COMPILE_SYNTAX);
    return 0;
}
