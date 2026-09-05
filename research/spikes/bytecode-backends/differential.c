#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "snapshot/src/re.h"

int baseline_match(const unsigned char *, const char *, const char **,
                   const void **, unsigned, unsigned);

static unsigned char *unhex(const char *hex)
{
    size_t length = strcmp(hex, "-") ? strlen(hex) / 2 : 0;
    unsigned char *bytes = malloc(length + 1);
    if (!bytes) exit(2);
    for (size_t index = 0; index < length; index++) {
        unsigned value;
        if (sscanf(hex + index * 2, "%2x", &value) != 1) exit(2);
        bytes[index] = value;
    }
    bytes[length] = 0;
    return bytes;
}

int main(void)
{
    char *line = NULL;
    size_t capacity = 0;
    while (getline(&line, &capacity, stdin) >= 0) {
        char *base_hex = strtok(line, " \t\n");
        char *test_hex = strtok(NULL, " \t\n");
        char *text_hex = strtok(NULL, " \t\n");
        char *steps_string = strtok(NULL, " \t\n");
        char *depth_string = strtok(NULL, " \t\n");
        if (!base_hex || !test_hex || !text_hex || !steps_string || !depth_string) return 2;
        unsigned steps = (unsigned)strtoul(steps_string, NULL, 10);
        unsigned depth = (unsigned)strtoul(depth_string, NULL, 10);
        unsigned char *base = unhex(base_hex), *test = unhex(test_hex);
        char *text = (char *)unhex(text_hex);
        const char **expected = calloc(base[0], sizeof(*expected));
        const char **actual = calloc(test[0], sizeof(*actual));
        size_t count = RE_WORKSPACE_SLOTS(base, depth);
        const void **workspace = count ? malloc(count * sizeof(*workspace)) : NULL;
        if (!expected || !actual || (count && !workspace) || base[0] != test[0]) return 2;
        int before = baseline_match(base, text, expected, workspace, steps, depth);
        int after = re_match(test, text, actual, workspace, steps, depth);
        int equal = before == after;
        if (before == RE_MATCH && after == RE_MATCH) {
            for (unsigned slot = 0; slot < base[0]; slot++) {
                if (expected[slot] != actual[slot]) equal = 0;
            }
        }
        printf("%d %d %d", before, after, equal);
        if (!equal && before >= 0 && after >= 0) {
            for (unsigned slot = 0; slot < base[0]; slot++) {
                printf(" %td:%td", expected[slot] ? expected[slot] - text : -1,
                       actual[slot] ? actual[slot] - text : -1);
            }
        }
        putchar('\n');
        free(workspace);
        free(expected);
        free(actual);
        free(base);
        free(test);
        free(text);
    }
    free(line);
    return 0;
}
