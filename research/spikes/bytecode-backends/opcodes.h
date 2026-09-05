#ifndef EXPERIMENT_OPCODES_H
#define EXPERIMENT_OPCODES_H

#include "snapshot/src/re_bytecode.h"

enum {
    RE_OP_CHAR = RE_OP_SAVE + 1,
    RE_OP_SPLIT_SHORT,
    RE_OP_JUMP_SHORT,
    RE_OP_CHAR_STAR,
    RE_OP_ASCII_RANGE
};

enum { RE_BRANCH_ESCAPE = 0x80 };

#endif
