/* driver.c - PROTOCOL.md CLI contract for QuickJS libregexp: ./engine_cli [-i] PATTERN TEXT
 * Adaptation: libregexp matches Latin-1 or UTF-16 buffers, not UTF-8, so the text is
 * transcoded to UTF-16 (code-point semantics incl. surrogate pairs in unicode mode) and
 * capture pointers are mapped back to UTF-8 byte offsets. Pattern is compiled with
 * LRE_FLAG_UNICODE first (needed for non-BMP literals such as emoji); if that is rejected
 * we retry without it (ES Annex-B lenient syntax) and say so on stderr. Search-anywhere and
 * captures are native (lre_exec scans all start positions itself). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "libregexp.h"
#include "cutils.h"

int lre_check_stack_overflow(void *opaque, size_t alloca_size) { return 0; }
int lre_check_timeout(void *opaque) { return 0; }
void *lre_realloc(void *opaque, void *ptr, size_t size) { return realloc(ptr, size); }

int main(int argc, char **argv) {
    int flags = 0, a = 1, len, n = 0, ret, i;
    char err[64];
    if (argc == 4 && !strcmp(argv[1], "-i")) { flags |= LRE_FLAG_IGNORECASE; a++; }
    if (argc - a != 2) { puts("error usage"); return 2; }
    const char *pat = argv[a];
    const uint8_t *text = (const uint8_t *)argv[a + 1], *p, *q;
    size_t tlen = strlen((const char *)text);
    uint16_t *u16 = malloc((tlen + 1) * sizeof *u16);   /* UTF-16 text */
    int *boff = malloc((tlen + 2) * sizeof *boff);        /* UTF-16 index -> byte offset */
    for (p = text; p < text + tlen; p = q) {
        int c = unicode_from_utf8(p, UTF8_CHAR_LEN_MAX, &q);
        if (c < 0) { c = *p; q = p + 1; }                  /* invalid byte -> Latin-1 */
        boff[n] = p - text;
        if (c >= 0x10000) { c -= 0x10000; u16[n++] = 0xD800 + (c >> 10); boff[n] = p - text; u16[n++] = 0xDC00 + (c & 0x3ff); }
        else u16[n++] = c;
    }
    boff[n] = tlen;
    uint8_t *bc = lre_compile(&len, err, sizeof err, pat, strlen(pat), flags | LRE_FLAG_UNICODE, NULL);
    if (!bc) {
        bc = lre_compile(&len, err, sizeof err, pat, strlen(pat), flags, NULL);
        if (bc) fprintf(stderr, "note: annexB-fallback (non-unicode parse)\n");
    }
    if (!bc) { printf("error %s\n", err); return 1; }
    uint8_t **cap = malloc(sizeof *cap * lre_get_alloc_count(bc));
    /* cbuf_type=1 (UTF-16 units); lre_exec upgrades it to 2 (surrogate pairs = 1 code point)
       when the bytecode carries LRE_FLAG_UNICODE. Passing 2 directly is NOT allowed: lre_exec
       computes cbuf_end = cbuf + (clen << cbuf_type), i.e. 2 means 4-byte units. */
    ret = lre_exec(cap, bc, (const uint8_t *)u16, 0, n, 1, NULL);
    if (ret != 1) { puts(ret == 0 ? "nomatch" : "error exec"); return 0; }
    for (i = 0; i < lre_get_capture_count(bc); i++) {
        printf(i ? "%d " : "match ", i);
        if (cap[2 * i] && cap[2 * i + 1])
            printf("%d %d\n", boff[(uint16_t *)cap[2 * i] - u16], boff[(uint16_t *)cap[2 * i + 1] - u16]);
        else puts("-1 -1");
    }
    return 0;
}
