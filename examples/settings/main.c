/* Parsing "key=value" configuration lines with a precompiled pattern.
 *
 * setting_re.h was generated with:
 *     node tools/re_compile.js '^(\w+)=(\d+)$' setting_re
 *
 * Nothing is allocated: both buffers are static and owned by this file. On a
 * 32-bit target they cost 24 bytes for the captures and 512 for the workspace.
 */
#include <stdio.h>
#include "re.h"
#include "setting_re.h"

enum { DEPTH = 16 };        /* maximum pending backtrack choice points */
enum { STEPS = 10000 };     /* budget: a pathological pattern errors, never hangs */

static const char *captures[setting_re_capture_slots];
static const void *workspace[(setting_re_capture_slots + 2u) * DEPTH];

int main(void)
{
    static const char *lines[] = {"timeout=30", "retries=5", "name=abc", "x=1"};

    for (unsigned i = 0; i < sizeof lines / sizeof *lines; i++) {
        const char *line = lines[i];
        int result = re_match(setting_re, line, captures, workspace, STEPS, DEPTH);

        if (result == RE_MATCH) {
            /* Pair 0 is the whole match, pair 1 the key, pair 2 the value. */
            printf("%-12s key=%.*s value=%.*s\n", line,
                   (int)(captures[3] - captures[2]), captures[2],
                   (int)(captures[5] - captures[4]), captures[4]);
        } else if (result == RE_NOMATCH) {
            printf("%-12s no match (%d)\n", line, result);
        } else {
            /* RE_BUDGET: raise STEPS. RE_SPACE: raise DEPTH. */
            printf("%-12s gave up (%d)\n", line, result);
        }
    }
    return 0;
}
