#include <stdio.h>
#include <string.h>

#include "re_compile.h"

int main(int argc, char **argv)
{
    static unsigned char output[65536];
    static struct re_compile_scratch scratch;
    unsigned flags = RE_COMPILE_SEARCH, index;
    int length;
    if (argc < 2 || argc > 3) return 2;
    if (argc == 3) {
        if (strchr(argv[2], 'a')) flags &= ~RE_COMPILE_SEARCH;
        if (strchr(argv[2], 'm')) flags |= RE_COMPILE_MULTILINE;
        if (strchr(argv[2], 's')) flags |= RE_COMPILE_DOTALL;
    }
    length = re_compile(output, sizeof(output), argv[1], flags, &scratch);
    if (length < 0) {
        puts("error");
        return 0;
    }
    for (index = 0; index < (unsigned)length; index++) printf("%02x", output[index]);
    putchar('\n');
    return 0;
}
