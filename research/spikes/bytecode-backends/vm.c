#include <stdint.h>
#include "snapshot/src/re.h"
#include "opcodes.h"

static uint32_t decode(const unsigned char **cursor)
{
    const unsigned char *position = *cursor;
    unsigned value = *position++, remaining;
    if (value < 128) remaining = 0;
    else {
        remaining = value < 224 ? 1 : value < 240 ? 2 : 3;
        value &= 127 >> (remaining + 1);
        while (remaining-- && *position) value = value << 6 | (*position++ & 63);
    }
    *cursor = position;
    return value;
}

static int16_t displacement(const unsigned char *position)
{
    return position[0] | position[1] << RE_BYTE_BITS;
}

#if ENABLE_ESCAPE
static const unsigned char *branch_target(const unsigned char **program)
{
    int relative = (int8_t)*(*program)++;
    if (relative == -(int)RE_BRANCH_ESCAPE) {
        relative = displacement(*program);
        *program += RE_OFFSET_BYTES;
    }
    return *program + relative;
}
#endif

static int word(unsigned value)
{
    return value == '_' || value - '0' < 10 || (value | 32) - 'a' < 26;
}

int re_match(const unsigned char *code, const char *text, const char **captures,
             const void **workspace, unsigned steps, unsigned workspace_depth)
{
    uint32_t character, lower, upper;
    const unsigned char *begin = (const unsigned char *)text, *program = code + 1;
    const unsigned char *input = begin, *next;
    const void **top = workspace;
    unsigned slots = *code, index, hit, count;
    for (index = slots; index--;) captures[index] = 0;
    for (;;) {
        if (!steps) return RE_BUDGET;
        --steps;
        switch (*program++) {
        case RE_OP_MATCH: return RE_MATCH;
        case RE_OP_BOL: if (input != begin) goto fail; break;
        case RE_OP_EOL: if (*input) goto fail; break;
        case RE_OP_WORD_BOUNDARY:
        case RE_OP_NOT_WORD_BOUNDARY:
            hit = input > begin && input[-1] < 128 && word(input[-1]);
            count = *input && *input < 128 && word(*input);
            if ((hit == count) == (program[-1] == RE_OP_WORD_BOUNDARY)) goto fail;
            break;
        case RE_OP_LINE_BOL: if (input != begin && input[-1] != '\n') goto fail; break;
        case RE_OP_LINE_EOL: if (*input && *input != '\n') goto fail; break;
        case RE_OP_CLASS:
#if ENABLE_CHAR
        case RE_OP_CHAR:
#endif
#if ENABLE_RANGE
        case RE_OP_ASCII_RANGE:
#endif
            if (!*input) goto fail;
            next = input;
            character = decode(&next);
#if ENABLE_CHAR
            if (program[-1] == RE_OP_CHAR) hit = character == *program++;
            else
#endif
#if ENABLE_RANGE
            if (program[-1] == RE_OP_ASCII_RANGE) {
                hit = character >= program[0] && character <= program[1];
                program += 2;
            } else
#endif
            {
            hit = *program++;
            count = hit & RE_CLASS_RANGE_MASK;
            hit >>= RE_CLASS_NEGATED_SHIFT;
            while (count--) {
                lower = decode(&program);
                upper = decode(&program);
                if (character >= lower && character <= upper) hit ^= 1;
            }
            }
            if (!hit) goto fail;
            input = next;
            break;
#if ENABLE_STAR
        case RE_OP_CHAR_STAR:
            if (!workspace_depth) return RE_SPACE;
            --workspace_depth;
            *top++ = program + 1;
            *top++ = input;
            for (index = 0; index < slots; index++) *top++ = captures[index];
            if (!*input || decode(&input) != *program) goto fail;
            --program;
            break;
#endif
#if ENABLE_SHORT && SHARED_BRANCH && !ENABLE_ESCAPE
        case RE_OP_SPLIT_SHORT:
        case RE_OP_JUMP_SHORT:
        case RE_OP_SPLIT:
        case RE_OP_JUMP:
            count = program[-1];
            if (count >= RE_OP_SPLIT_SHORT) {
                next = program + 1 + (int8_t)*program;
                ++program;
            } else {
                next = program + RE_OFFSET_BYTES + displacement(program);
                program += RE_OFFSET_BYTES;
            }
            if (count == RE_OP_JUMP || count == RE_OP_JUMP_SHORT) {
                program = next;
                break;
            }
#else
#if ENABLE_SHORT && !ENABLE_ESCAPE
        case RE_OP_SPLIT_SHORT:
            next = program + 1 + (int8_t)*program;
            ++program;
            goto save_choice;
        case RE_OP_JUMP_SHORT:
            program = program + 1 + (int8_t)*program;
            break;
#endif
        case RE_OP_SPLIT:
#if ENABLE_ESCAPE
            next = branch_target(&program);
#else
            next = program + RE_OFFSET_BYTES + displacement(program);
            program += RE_OFFSET_BYTES;
#endif
#if ENABLE_SHORT && !ENABLE_ESCAPE
        save_choice:
#endif
#endif
            if (!workspace_depth) return RE_SPACE;
            --workspace_depth;
            *top++ = next;
            *top++ = input;
            for (index = 0; index < slots; index++) *top++ = captures[index];
            break;
#if !(ENABLE_SHORT && SHARED_BRANCH) || ENABLE_ESCAPE
        case RE_OP_JUMP:
#if ENABLE_ESCAPE
            program = branch_target(&program);
#else
            program = program + RE_OFFSET_BYTES + displacement(program);
#endif
            break;
#endif
        case RE_OP_SAVE: captures[*program++] = (const char *)input; break;
        }
        continue;
    fail:
        if (top == workspace) return RE_NOMATCH;
        top -= slots + 2;
        program = top[0];
        input = top[1];
        for (index = 0; index < slots; index++) captures[index] = top[index + 2];
        ++workspace_depth;
    }
}
