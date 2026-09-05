#include <stdint.h>
#include "opcodes.h"
#include "repack.h"

enum {
    REPACK_HEADER = 1,
    REPACK_SHORT_BYTES = 2,
    REPACK_SHORT_MIN = -128,
    REPACK_SHORT_MAX = 127,
    REPACK_LONG_MIN = -32768,
    REPACK_LONG_MAX = 32767,
    REPACK_SPACE = -2,
    REPACK_RANGE = -3
};

static unsigned value_width(unsigned value)
{
    return value < 128 ? 1 : value < 224 ? 2 : value < 240 ? 3 : 4;
}

static unsigned layout(struct repack_record *scratch, unsigned count)
{
    unsigned offset = REPACK_HEADER;
    for (unsigned index = 0; index < count; index++) {
        scratch[index].destination = offset;
        offset += scratch[index].width;
    }
    return offset;
}

int re_repack(const unsigned char *input, unsigned length, unsigned char *output,
              unsigned capacity, struct repack_record *scratch, unsigned records)
{
    unsigned count = 0, source = REPACK_HEADER, total;
    while (source < length) {
        if (count == records) return REPACK_SPACE;
        struct repack_record *record = scratch + count++;
        record->source = source;
        record->opcode = input[source++];
        record->target = -1;
        switch (record->opcode) {
        case RE_OP_CLASS: {
            unsigned ranges = input[source++] & RE_CLASS_RANGE_MASK;
            for (unsigned bound = 0; bound < ranges * 2; bound++) source += value_width(input[source]);
            break;
        }
        case RE_OP_SPLIT:
        case RE_OP_JUMP:
            record->target = (int)(source + RE_OFFSET_BYTES) + (int16_t)(input[source] | input[source + 1] << RE_BYTE_BITS);
            source += RE_OFFSET_BYTES;
            break;
        case RE_OP_SAVE:
        case RE_OP_CHAR: source++; break;
        case RE_OP_ASCII_RANGE: source += 2; break;
        }
        record->end = source;
        record->width = source - record->source;
#if ENABLE_ESCAPE
        if (record->target >= 0) record->width++;
#endif
    }
    for (unsigned index = 0; index < count; index++) {
        struct repack_record *record = scratch + index;
        if (record->target < 0) continue;
        unsigned target = 0;
        while (target < count && scratch[target].source != (unsigned)record->target) target++;
        if (target == count) return REPACK_RANGE;
        record->target = (int)target;
    }
#if ENABLE_STAR
    for (unsigned index = 0; index + 3 < count; index++) {
        struct repack_record *record = scratch + index;
        if (record->opcode != RE_OP_SPLIT || record->target != (int)index + 3 ||
            record[1].opcode != RE_OP_CHAR || record[2].opcode != RE_OP_JUMP ||
            record[2].target != (int)index) continue;
        unsigned incoming = 0;
        for (unsigned other = 0; other < count; other++) {
            if (scratch[other].target == (int)index + 1 || scratch[other].target == (int)index + 2) incoming++;
        }
        if (incoming) continue;
        record->opcode = RE_OP_CHAR_STAR;
        record->source = record[1].source;
        record->end = record[1].end;
        record->width = record[1].width;
        record->target = -1;
        record[1].width = record[2].width = 0;
        record[1].target = record[2].target = -1;
    }
#endif
#if ENABLE_SHORT
    unsigned changed;
    do {
        changed = 0;
        layout(scratch, count);
        for (unsigned index = 0; index < count; index++) {
            struct repack_record *record = scratch + index;
            if (record->target < 0 || record->width == REPACK_SHORT_BYTES) continue;
            unsigned target = scratch[record->target].destination;
            int relative = (int)target - (int)record->destination - REPACK_SHORT_BYTES -
                (target > record->destination ? (int)record->width - REPACK_SHORT_BYTES : 0);
            if (relative >= REPACK_SHORT_MIN + ENABLE_ESCAPE && relative <= REPACK_SHORT_MAX) {
                record->width = REPACK_SHORT_BYTES;
#if !ENABLE_ESCAPE
                record->opcode = record->opcode == RE_OP_SPLIT ? RE_OP_SPLIT_SHORT : RE_OP_JUMP_SHORT;
#endif
                changed = 1;
            }
        }
    } while (changed);
#endif
    total = layout(scratch, count);
    if (total > capacity) return REPACK_SPACE;
    output[0] = input[0];
    for (unsigned index = 0; index < count; index++) {
        const struct repack_record *record = scratch + index;
        if (!record->width) continue;
        unsigned destination = record->destination;
        output[destination++] = record->opcode;
        if (record->target < 0) {
            for (source = record->source + 1; source < record->end; source++) output[destination++] = input[source];
        } else {
            int relative = (int)scratch[record->target].destination - (int)record->destination - (int)record->width;
            if (relative < REPACK_LONG_MIN || relative > REPACK_LONG_MAX) return REPACK_RANGE;
#if ENABLE_ESCAPE
            if (record->width > REPACK_SHORT_BYTES) output[destination++] = RE_BRANCH_ESCAPE;
#endif
            output[destination++] = (unsigned)relative;
            if (record->width > REPACK_SHORT_BYTES) output[destination] = (unsigned)relative >> RE_BYTE_BITS;
        }
    }
    return (int)total;
}
