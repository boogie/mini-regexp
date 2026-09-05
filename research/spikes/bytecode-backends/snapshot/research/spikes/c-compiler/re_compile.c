#include "re_compile.h"
#include "../../../src/re_bytecode.h"

enum {
    RE_BYTECODE_HEADER_BYTES = 1,
    RE_SAVE_BYTES = 2,
    RE_LAZY_PREFIX_BYTES = 2 * RE_BRANCH_BYTES,
    RE_OVERALL_CAPTURE_PAIRS = 1,
    RE_CAPTURE_END_OFFSET = 1,
    RE_OFFSET_MIN = -0x8000,
    RE_OFFSET_MAX = 0x7fff,
    RE_ASCII_LIMIT = 0x80,
    RE_UTF8_TWO_BYTE_LIMIT = 0x800,
    RE_UTF8_THREE_BYTE_LIMIT = 0x10000,
    RE_UTF8_TWO_BYTE_PREFIX = 0xc0,
    RE_UTF8_THREE_BYTE_PREFIX = 0xe0,
    RE_UTF8_FOUR_BYTE_PREFIX = 0xf0,
    RE_UTF8_CONTINUATION_PREFIX = 0x80,
    RE_UTF8_CONTINUATION_MASK = 0x3f,
    RE_UTF8_HEAD_MASK = 0x7f,
    RE_ASCII_CASE_BIT = 0x20,
    RE_UNICODE_MAX = 0x10ffff
};

struct compiler {
    unsigned char *output;
    unsigned capacity;
    unsigned length;
    const unsigned char *pattern;
    unsigned flags;
    unsigned captures;
    int error;
    struct re_compile_scratch *scratch;
};

struct fragment {
    unsigned start;
    unsigned nullable;
};

static void byte(struct compiler *compiler, unsigned value)
{
    if (compiler->length == compiler->capacity) compiler->error = RE_COMPILE_SPACE;
    else compiler->output[compiler->length++] = value;
}

static void offset(struct compiler *compiler, unsigned position, int value)
{
    if (value < RE_OFFSET_MIN || value > RE_OFFSET_MAX) compiler->error = RE_COMPILE_RANGE;
    else {
        compiler->output[position] = value;
        compiler->output[position + 1] = (unsigned)value >> RE_BYTE_BITS;
    }
}

static void insert(struct compiler *compiler, unsigned position, unsigned count)
{
    unsigned index;
    if (count > compiler->capacity - compiler->length) {
        compiler->error = RE_COMPILE_SPACE;
        return;
    }
    for (index = compiler->length; index-- > position;)
        compiler->output[index + count] = compiler->output[index];
    compiler->length += count;
}

static void copy(struct compiler *compiler, unsigned position, unsigned count)
{
    unsigned index;
    for (index = 0; index < count && !compiler->error; index++)
        byte(compiler, compiler->output[position + index]);
}

static uint32_t codepoint(struct compiler *compiler)
{
    const unsigned char *pattern = compiler->pattern;
    uint32_t value = *pattern++;
    unsigned count;
    if (value < RE_ASCII_LIMIT) count = 0;
    else {
        count = value < RE_UTF8_THREE_BYTE_PREFIX ? 1 : value < RE_UTF8_FOUR_BYTE_PREFIX ? 2 : 3;
        value &= RE_UTF8_HEAD_MASK >> (count + 1);
        while (count-- && *pattern)
            value = value << 6 | (*pattern++ & RE_UTF8_CONTINUATION_MASK);
    }
    compiler->pattern = pattern;
    return value;
}

static void utf8(struct compiler *compiler, uint32_t value)
{
    if (value < RE_ASCII_LIMIT) byte(compiler, value);
    else if (value < RE_UTF8_TWO_BYTE_LIMIT) {
        byte(compiler, RE_UTF8_TWO_BYTE_PREFIX | value >> 6);
        byte(compiler, RE_UTF8_CONTINUATION_PREFIX | (value & RE_UTF8_CONTINUATION_MASK));
    } else if (value < RE_UTF8_THREE_BYTE_LIMIT) {
        byte(compiler, RE_UTF8_THREE_BYTE_PREFIX | value >> 12);
        byte(compiler, RE_UTF8_CONTINUATION_PREFIX |
             (value >> 6 & RE_UTF8_CONTINUATION_MASK));
        byte(compiler, RE_UTF8_CONTINUATION_PREFIX | (value & RE_UTF8_CONTINUATION_MASK));
    } else {
        byte(compiler, RE_UTF8_FOUR_BYTE_PREFIX | value >> 18);
        byte(compiler, RE_UTF8_CONTINUATION_PREFIX |
             (value >> 12 & RE_UTF8_CONTINUATION_MASK));
        byte(compiler, RE_UTF8_CONTINUATION_PREFIX |
             (value >> 6 & RE_UTF8_CONTINUATION_MASK));
        byte(compiler, RE_UTF8_CONTINUATION_PREFIX | (value & RE_UTF8_CONTINUATION_MASK));
    }
}

static void range(struct compiler *compiler, unsigned *count, uint32_t lower, uint32_t upper)
{
    struct re_compile_range value;
    unsigned index = *count;
    if (index == RE_COMPILE_MAX_RANGES) {
        compiler->error = RE_COMPILE_RANGE;
        return;
    }
    while (index && compiler->scratch->ranges[index - 1].lower > lower) {
        compiler->scratch->ranges[index] = compiler->scratch->ranges[index - 1];
        index--;
    }
    value.lower = lower;
    value.upper = upper;
    compiler->scratch->ranges[index] = value;
    (*count)++;
}

static unsigned merge(struct compiler *compiler, unsigned count)
{
    unsigned input, output = 0;
    for (input = 0; input < count; input++) {
        struct re_compile_range *current = &compiler->scratch->ranges[input];
        if (output && current->lower <= compiler->scratch->ranges[output - 1].upper + 1) {
            if (current->upper > compiler->scratch->ranges[output - 1].upper)
                compiler->scratch->ranges[output - 1].upper = current->upper;
        } else compiler->scratch->ranges[output++] = *current;
    }
    return output;
}

static void base_ranges(struct compiler *compiler, unsigned *count, unsigned shorthand,
                        unsigned complement)
{
    static const uint32_t digit[] = {'0', '9'};
    static const uint32_t word[] = {'0', '9', 'A', 'Z', '_', '_', 'a', 'z'};
    static const uint32_t space[] = {'\t', '\r', ' ', ' '};
    const uint32_t *values = shorthand == 'd' ? digit : shorthand == 'w' ? word : space;
    unsigned pairs = shorthand == 'd' ? 1 : shorthand == 'w' ? 4 : 2;
    unsigned index;
    uint32_t position = 0;
    if (!complement) {
        for (index = 0; index < pairs; index++)
            range(compiler, count, values[index * 2], values[index * 2 + 1]);
        return;
    }
    for (index = 0; index < pairs; index++) {
        if (position < values[index * 2]) range(compiler, count, position, values[index * 2] - 1);
        position = values[index * 2 + 1] + 1;
    }
    if (position <= RE_UNICODE_MAX) range(compiler, count, position, RE_UNICODE_MAX);
}

static void emit_class(struct compiler *compiler, unsigned count, unsigned negated)
{
    unsigned index;
    count = merge(compiler, count);
    byte(compiler, RE_OP_CLASS);
    byte(compiler, count | (negated ? RE_CLASS_NEGATED : 0));
    for (index = 0; index < count; index++) {
        utf8(compiler, compiler->scratch->ranges[index].lower);
        utf8(compiler, compiler->scratch->ranges[index].upper);
    }
}

struct class_atom {
    uint32_t value;
    unsigned single;
};

static struct class_atom class_atom(struct compiler *compiler, unsigned *count)
{
    struct class_atom atom;
    unsigned escaped;
    atom.single = 1;
    if (*compiler->pattern == '\\') {
        compiler->pattern++;
        escaped = codepoint(compiler);
        if (escaped == 'd' || escaped == 'D' || escaped == 'w' || escaped == 'W' ||
            escaped == 's' || escaped == 'S') {
            base_ranges(compiler, count, escaped | RE_ASCII_CASE_BIT,
                        !(escaped & RE_ASCII_CASE_BIT));
            atom.single = 0;
            atom.value = 0;
            return atom;
        }
        atom.value = escaped;
    } else atom.value = codepoint(compiler);
    return atom;
}

static struct fragment expression(struct compiler *compiler);

static struct fragment character_class(struct compiler *compiler, unsigned start)
{
    unsigned count = 0, negated = *compiler->pattern == '^';
    struct class_atom first, second;
    if (negated) compiler->pattern++;
    while (*compiler->pattern && *compiler->pattern != ']') {
        first = class_atom(compiler, &count);
        if (first.single && *compiler->pattern == '-' && compiler->pattern[1] &&
            compiler->pattern[1] != ']') {
            compiler->pattern++;
            second = class_atom(compiler, &count);
            if (!second.single) compiler->error = RE_COMPILE_SYNTAX;
            else range(compiler, &count, first.value, second.value);
        } else if (first.single) range(compiler, &count, first.value, first.value);
    }
    if (*compiler->pattern != ']') compiler->error = RE_COMPILE_SYNTAX;
    else compiler->pattern++;
    emit_class(compiler, count, negated);
    return (struct fragment){start, 0};
}

static struct fragment literal(struct compiler *compiler, unsigned start, uint32_t value)
{
    range(compiler, &(unsigned){0}, value, value);
    emit_class(compiler, 1, 0);
    return (struct fragment){start, 0};
}

static struct fragment atom(struct compiler *compiler)
{
    unsigned start = compiler->length, character = codepoint(compiler), group, escaped, count;
    struct fragment fragment;
    if (character == '.') {
        count = 0;
        if (!(compiler->flags & RE_COMPILE_DOTALL)) range(compiler, &count, '\n', '\n');
        emit_class(compiler, count, 1);
        return (struct fragment){start, 0};
    }
    if (character == '^') {
        byte(compiler, compiler->flags & RE_COMPILE_MULTILINE ? RE_OP_LINE_BOL : RE_OP_BOL);
        return (struct fragment){start, 1};
    }
    if (character == '$') {
        byte(compiler, compiler->flags & RE_COMPILE_MULTILINE ? RE_OP_LINE_EOL : RE_OP_EOL);
        return (struct fragment){start, 1};
    }
    if (character == '(') {
        if (compiler->pattern[0] == '?' && compiler->pattern[1] == ':') {
            compiler->pattern += 2;
            group = 0;
        } else {
            group = ++compiler->captures;
            byte(compiler, RE_OP_SAVE);
            byte(compiler, group * RE_CAPTURE_PAIR_SLOTS);
        }
        fragment = expression(compiler);
        if (*compiler->pattern != ')') compiler->error = RE_COMPILE_SYNTAX;
        else compiler->pattern++;
        if (group) {
            byte(compiler, RE_OP_SAVE);
            byte(compiler, group * RE_CAPTURE_PAIR_SLOTS + RE_CAPTURE_END_OFFSET);
        }
        fragment.start = start;
        return fragment;
    }
    if (character == '[') return character_class(compiler, start);
    if (character == '\\') {
        escaped = codepoint(compiler);
        if (escaped == 'b' || escaped == 'B' || escaped == 'A' || escaped == 'z') {
            byte(compiler, escaped == 'b' ? RE_OP_WORD_BOUNDARY :
                 escaped == 'B' ? RE_OP_NOT_WORD_BOUNDARY :
                 escaped == 'A' ? RE_OP_BOL : RE_OP_EOL);
            return (struct fragment){start, 1};
        }
        if (escaped == 'd' || escaped == 'D' || escaped == 'w' || escaped == 'W' ||
            escaped == 's' || escaped == 'S') {
            count = 0;
            base_ranges(compiler, &count, escaped | RE_ASCII_CASE_BIT, 0);
            emit_class(compiler, count, !(escaped & RE_ASCII_CASE_BIT));
            return (struct fragment){start, 0};
        }
        if (escaped == 'n') escaped = '\n';
        else if (escaped == 'r') escaped = '\r';
        else if (escaped == 't') escaped = '\t';
        else if (escaped == 'f') escaped = '\f';
        else if (escaped == 'v') escaped = '\v';
        return literal(compiler, start, escaped);
    }
    if (!character) compiler->error = RE_COMPILE_SYNTAX;
    return literal(compiler, start, character);
}

static unsigned number(struct compiler *compiler)
{
    unsigned value = 0, found = 0;
    while (*compiler->pattern >= '0' && *compiler->pattern <= '9') {
        found = 1;
        value = value * 10 + *compiler->pattern++ - '0';
    }
    if (!found) compiler->error = RE_COMPILE_SYNTAX;
    return value;
}

static void optional(struct compiler *compiler, unsigned start, unsigned length, unsigned lazy)
{
    insert(compiler, start, lazy ? RE_LAZY_PREFIX_BYTES : RE_BRANCH_BYTES);
    if (compiler->error) return;
    compiler->output[start] = RE_OP_SPLIT;
    if (lazy) {
        offset(compiler, start + 1, RE_BRANCH_BYTES);
        compiler->output[start + RE_BRANCH_BYTES] = RE_OP_JUMP;
        offset(compiler, start + RE_BRANCH_BYTES + 1, length);
    } else offset(compiler, start + 1, length);
}

static void star(struct compiler *compiler, unsigned start, unsigned length, unsigned lazy)
{
    unsigned prefix = lazy ? RE_LAZY_PREFIX_BYTES : RE_BRANCH_BYTES;
    insert(compiler, start, prefix);
    if (compiler->error) return;
    compiler->output[start] = RE_OP_SPLIT;
    if (lazy) {
        offset(compiler, start + 1, RE_BRANCH_BYTES);
        compiler->output[start + RE_BRANCH_BYTES] = RE_OP_JUMP;
        offset(compiler, start + RE_BRANCH_BYTES + 1, length + RE_BRANCH_BYTES);
    } else offset(compiler, start + 1, length + RE_BRANCH_BYTES);
    byte(compiler, RE_OP_JUMP);
    byte(compiler, 0);
    byte(compiler, 0);
    offset(compiler, compiler->length - RE_OFFSET_BYTES, -(int)(prefix + length + RE_BRANCH_BYTES));
}

static struct fragment quantified(struct compiler *compiler, struct fragment fragment)
{
    unsigned character = *compiler->pattern, lower, upper, finite = 1, lazy, index;
    unsigned original_length = compiler->length - fragment.start, source = fragment.start;
    if (character != '*' && character != '+' && character != '?' && character != '{')
        return fragment;
    if (character == '*') {
        compiler->pattern++;
        lower = 0;
        finite = 0;
        upper = 0;
    } else if (character == '+') {
        compiler->pattern++;
        lower = 1;
        finite = 0;
        upper = 0;
    } else if (character == '?') {
        compiler->pattern++;
        lower = 0;
        upper = 1;
    } else {
        compiler->pattern++;
        lower = upper = number(compiler);
        if (*compiler->pattern == ',') {
            compiler->pattern++;
            if (*compiler->pattern == '}') finite = 0;
            else upper = number(compiler);
        }
        if (*compiler->pattern != '}') compiler->error = RE_COMPILE_SYNTAX;
        else compiler->pattern++;
        if (finite && upper < lower) compiler->error = RE_COMPILE_SYNTAX;
    }
    lazy = *compiler->pattern == '?';
    if (lazy) compiler->pattern++;
    if (!finite && fragment.nullable) compiler->error = RE_COMPILE_SYNTAX;
    if (compiler->error) return fragment;
    if (!lower) {
        if (finite && !upper) compiler->length = fragment.start;
        else if (finite) {
            optional(compiler, fragment.start, original_length, lazy);
            source += lazy ? RE_LAZY_PREFIX_BYTES : RE_BRANCH_BYTES;
            for (index = 1; index < upper; index++) {
                unsigned position = compiler->length;
                copy(compiler, source, original_length);
                optional(compiler, position, original_length, lazy);
            }
        } else star(compiler, fragment.start, original_length, lazy);
    } else {
        for (index = 1; index < lower; index++) copy(compiler, source, original_length);
        if (finite) {
            for (index = lower; index < upper; index++) {
                unsigned position = compiler->length;
                copy(compiler, source, original_length);
                optional(compiler, position, original_length, lazy);
            }
        } else {
            unsigned position = compiler->length;
            copy(compiler, source, original_length);
            star(compiler, position, original_length, lazy);
        }
    }
    fragment.nullable = !lower || fragment.nullable;
    return fragment;
}

static struct fragment sequence(struct compiler *compiler)
{
    struct fragment result = {compiler->length, 1}, item;
    while (*compiler->pattern && *compiler->pattern != ')' && *compiler->pattern != '|') {
        item = quantified(compiler, atom(compiler));
        result.nullable &= item.nullable;
    }
    return result;
}

static struct fragment expression(struct compiler *compiler)
{
    struct fragment left = sequence(compiler), right;
    unsigned left_length, jump;
    if (*compiler->pattern != '|') return left;
    compiler->pattern++;
    left_length = compiler->length - left.start;
    insert(compiler, left.start, RE_BRANCH_BYTES);
    if (compiler->error) return left;
    compiler->output[left.start] = RE_OP_SPLIT;
    byte(compiler, RE_OP_JUMP);
    byte(compiler, 0);
    byte(compiler, 0);
    jump = compiler->length - RE_OFFSET_BYTES;
    right = expression(compiler);
    offset(compiler, left.start + 1, left_length + RE_BRANCH_BYTES);
    offset(compiler, jump, compiler->length - jump - RE_OFFSET_BYTES);
    left.nullable |= right.nullable;
    return left;
}

int re_compile(unsigned char *output, unsigned capacity, const char *pattern, unsigned flags,
               struct re_compile_scratch *scratch)
{
    struct compiler compiler = {output, capacity, RE_BYTECODE_HEADER_BYTES,
                                (const unsigned char *)pattern, flags, 0, 0, scratch};
    struct fragment fragment;
    unsigned core_length;
    if (!capacity) return RE_COMPILE_SPACE;
    fragment = expression(&compiler);
    if (*compiler.pattern) compiler.error = RE_COMPILE_SYNTAX;
    if (compiler.error) return compiler.error;
    insert(&compiler, fragment.start, RE_SAVE_BYTES);
    if (compiler.error) return compiler.error;
    compiler.output[fragment.start] = RE_OP_SAVE;
    compiler.output[fragment.start + RE_CAPTURE_END_OFFSET] = 0;
    byte(&compiler, RE_OP_SAVE);
    byte(&compiler, RE_CAPTURE_END_OFFSET);
    byte(&compiler, RE_OP_MATCH);
    core_length = compiler.length - fragment.start;
    if (flags & RE_COMPILE_SEARCH) {
        insert(&compiler, fragment.start, RE_BRANCH_BYTES);
        if (compiler.error) return compiler.error;
        compiler.output[fragment.start] = RE_OP_SPLIT;
        offset(&compiler, fragment.start + 1, core_length);
        byte(&compiler, RE_OP_CLASS);
        byte(&compiler, RE_CLASS_NEGATED);
        byte(&compiler, RE_OP_JUMP);
        byte(&compiler, 0);
        byte(&compiler, 0);
        offset(&compiler, compiler.length - RE_OFFSET_BYTES,
               -(int)(compiler.length - RE_BYTECODE_HEADER_BYTES));
    }
    core_length = (compiler.captures + RE_OVERALL_CAPTURE_PAIRS) * RE_CAPTURE_PAIR_SLOTS;
    if (core_length > RE_MAX_CAPTURE_SLOTS) compiler.error = RE_COMPILE_RANGE;
    else compiler.output[0] = core_length;
    return compiler.error ? compiler.error : (int)compiler.length;
}
