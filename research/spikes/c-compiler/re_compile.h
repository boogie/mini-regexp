#ifndef MINI_RE_COMPILE_H
#define MINI_RE_COMPILE_H

#include <stdint.h>

enum {
    RE_COMPILE_SEARCH = 1,
    RE_COMPILE_MULTILINE = 2,
    RE_COMPILE_DOTALL = 4
};

enum {
    RE_COMPILE_SYNTAX = -1,
    RE_COMPILE_SPACE = -2,
    RE_COMPILE_RANGE = -3
};

enum { RE_COMPILE_MAX_RANGES = 127 };

struct re_compile_range {
    uint32_t lower;
    uint32_t upper;
};

struct re_compile_scratch {
    struct re_compile_range ranges[RE_COMPILE_MAX_RANGES];
};

int re_compile(unsigned char *output, unsigned capacity, const char *pattern, unsigned flags,
               struct re_compile_scratch *scratch);

#endif
