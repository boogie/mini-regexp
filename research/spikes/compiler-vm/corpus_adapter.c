#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../src/re.h"

static int hex_digit(char character)
{
    if (character >= '0' && character <= '9') return character - '0';
    if (character >= 'a' && character <= 'f') return character - 'a' + 10;
    if (character >= 'A' && character <= 'F') return character - 'A' + 10;
    return -1;
}

int main(int argc, char **argv)
{
    size_t hex_length, index;
    unsigned char *program;
    const char **captures;
    const void **workspace;
    int high, low, result;
    unsigned slot, slot_count;

    if (argc != 3) {
        puts("error");
        return 2;
    }
    hex_length = strlen(argv[1]);
    if (hex_length == 0 || hex_length % 2 != 0) {
        puts("error");
        return 0;
    }
    program = malloc(hex_length / 2);
    if (!program) {
        puts("error");
        return 0;
    }
    for (index = 0; index < hex_length; index += 2) {
        high = hex_digit(argv[1][index]);
        low = hex_digit(argv[1][index + 1]);
        if (high < 0 || low < 0) {
            free(program);
            puts("error");
            return 0;
        }
        program[index / 2] = (unsigned char)(high << 4 | low);
    }

    slot_count = program[0];
    captures = malloc(slot_count * sizeof(*captures));
    if (!captures) {
        free(program);
        puts("error");
        return 0;
    }
    workspace = malloc(RE_WORKSPACE_SLOTS(program, 4096) * sizeof(*workspace));
    if (!workspace) {
        free(captures);
        free(program);
        puts("error");
        return 0;
    }

    result = re_match(program, argv[2], captures, workspace, 1000000, 4096);
    if (result > 0) {
        printf("match %d %d\n", (int)(captures[0] - argv[2]),
               (int)(captures[1] - argv[2]));
        for (slot = 2; slot < slot_count; slot += 2) {
            if (captures[slot] && captures[slot + 1])
                printf("%u %d %d\n", slot / 2, (int)(captures[slot] - argv[2]),
                       (int)(captures[slot + 1] - argv[2]));
            else
                printf("%u -1 -1\n", slot / 2);
        }
    } else if (result == 0) {
        puts("nomatch");
    } else {
        puts("error");
    }
    free(workspace);
    free(captures);
    free(program);
    return 0;
}
