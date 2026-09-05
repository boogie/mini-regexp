#include <assert.h>
#include <string.h>
#include "opcodes.h"
#include "repack.h"

int main(void)
{
    const unsigned char program[] = {2, RE_OP_SAVE, 0, RE_OP_MATCH};
    const unsigned char invalid_target[] = {2, RE_OP_JUMP, 0, 0};
    struct repack_record records[sizeof program];
    unsigned char output[sizeof program];
    assert(re_repack(program, sizeof program, NULL, 0, records, sizeof program) == -2);
    assert(re_repack(program, sizeof program, output, sizeof output, NULL, 0) == -2);
    assert(re_repack(program, sizeof program, output, sizeof output - 1, records, sizeof program) == -2);
    assert(re_repack(program, sizeof program, output, sizeof output, records, sizeof program) == sizeof program);
    assert(!memcmp(program, output, sizeof program));
    assert(re_repack(invalid_target, sizeof invalid_target, output, sizeof output, records, sizeof program) == -3);
    return 0;
}
