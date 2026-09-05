#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "snapshot/research/spikes/c-compiler/re_compile.h"
#if USE_REPACK
#include "repack.h"
#endif

enum { TEST_COMPILER_CAPACITY = 65536 };

static unsigned char *unhex(const char *hex, size_t *length)
{
    *length = strcmp(hex, "-") ? strlen(hex) / 2 : 0;
    unsigned char *bytes = malloc(*length + 1);
    if (!bytes) exit(2);
    for (size_t index = 0; index < *length; index++) {
        unsigned value;
        if (sscanf(hex + index * 2, "%2x", &value) != 1) exit(2);
        bytes[index] = value;
    }
    bytes[*length] = 0;
    return bytes;
}

int main(void)
{
    char *line = NULL;
    size_t line_capacity = 0;
    while (getline(&line, &line_capacity, stdin) >= 0) {
        char *pattern_hex = strtok(line, " \t\n");
        char *expected_hex = strtok(NULL, " \t\n");
        char *flags_string = strtok(NULL, " \t\n");
        if (!pattern_hex || !expected_hex || !flags_string) return 2;
        size_t pattern_length, expected_length;
        unsigned char *pattern = unhex(pattern_hex, &pattern_length);
        unsigned char *expected = unhex(expected_hex, &expected_length);
        unsigned flags = (unsigned)strtoul(flags_string, NULL, 10);
        size_t output_capacity = expected_length > TEST_COMPILER_CAPACITY ? expected_length : TEST_COMPILER_CAPACITY;
        unsigned char *actual = malloc(output_capacity);
        struct re_compile_scratch scratch;
        if (!actual) return 2;
        int result = re_compile(actual, (unsigned)output_capacity, (const char *)pattern, flags, &scratch);
#if USE_REPACK
        if (result > 0) {
            unsigned char *packed = malloc(output_capacity);
            struct repack_record *records = malloc((size_t)result * sizeof(*records));
            if (!packed || !records) return 2;
            result = re_repack(actual, (unsigned)result, packed, (unsigned)output_capacity, records, (unsigned)result);
            free(actual);
            free(records);
            actual = packed;
        }
#endif
        int equal = result == (int)expected_length && !memcmp(expected, actual, expected_length);
        printf("%d %d\n", result, equal);
        free(actual);
        free(expected);
        free(pattern);
    }
    free(line);
    return 0;
}
