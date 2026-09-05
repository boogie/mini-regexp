/* instr.c -- -finstrument-functions probe.
 * Records (a) the lowest observed stack address and (b) the exact call-stack
 * composition (which function, how many frames) at that deepest moment. */
#include <stddef.h>

#define SHADOW_MAX 200000

char *g_stack_hi;
char *g_stack_lo;
unsigned long g_calls;

void *g_shadow[SHADOW_MAX];
int   g_depth;
void *g_peak_stack[SHADOW_MAX];   /* snapshot of the shadow stack at peak */
int   g_peak_depth;

__attribute__((no_instrument_function))
static void note(char *probe)
{
    int i;
    if (probe < g_stack_lo) {
        g_stack_lo = probe;
        g_peak_depth = g_depth;
        for (i = 0; i < g_depth && i < SHADOW_MAX; i++) g_peak_stack[i] = g_shadow[i];
    }
}

__attribute__((no_instrument_function))
void __cyg_profile_func_enter(void *this_fn, void *call_site)
{
    char probe;
    (void)call_site;
    g_calls++;
    if (g_depth < SHADOW_MAX) g_shadow[g_depth] = this_fn;
    g_depth++;
    note(&probe);
}

__attribute__((no_instrument_function))
void __cyg_profile_func_exit(void *this_fn, void *call_site)
{
    char probe;
    (void)this_fn; (void)call_site;
    note(&probe);
    if (g_depth > 0) g_depth--;
}
