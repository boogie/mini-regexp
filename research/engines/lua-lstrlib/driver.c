/* Host driver for the Lua 5.4 pattern matcher (lstrlib.c) - PROTOCOL.md CLI contract.
 *   ./engine_cli [-i] PATTERN TEXT
 * PATTERN is Perl/JS-style regex syntax (the corpus), Lua patterns are a different
 * language, so this file does a purely SYNTACTIC 1:1 translation into Lua pattern
 * syntax and prints "error unsupported:<feature>" for anything Lua patterns cannot
 * express (alternation, {n,m}, quantified groups, lazy ??, \b, lookaround, ...).
 * No extra matching logic is added: the search loop below is a copy of str_find_aux.
 * Includes lstrlib.c directly to reach the static match()/MatchState. */
#include "lua/lstrlib.c"

static char out[16384]; static int n;              /* translated Lua pattern */
static const char *why;                            /* unsupported feature name */
static void emit(int c) { out[n++] = (char)c; }
static int fail(const char *w) { why = w; return -1; }

/* literal byte: '%'-escape every ASCII non-alnum (always legal in Lua, but NOT for
 * range endpoints, see class parser); with -i letters become [xX] / xX. */
static void lit(int c, int inclass, int ci) {
  if (ci && isalpha(c)) {
    if (!inclass) emit('[');
    emit(tolower(c)); emit(toupper(c));
    if (!inclass) emit(']');
  } else {
    if (c < 128 && !isalnum(c) && c != 0) emit('%');
    emit(c);
  }
}
static int hexv(int c) { return isdigit(c) ? c - '0' : (tolower(c) - 'a' + 10); }
/* decode one escape at p[i] (p[i]=='\\'): returns byte, or -1 for a class escape
 * (sets *cls to the letter), advances *ip past the escape. */
static int escape(const unsigned char *p, int *ip, int *cls) {
  int c = p[*ip + 1]; *ip += 2; *cls = 0;
  switch (c) {
    case 'd': case 'w': case 's': case 'D': case 'W': case 'S': *cls = c; return -1;
    case 'n': return '\n'; case 't': return '\t'; case 'r': return '\r';
    case 'f': return '\f'; case 'v': return '\v'; case '0': return 0;
    case 'x': if (isxdigit(p[*ip]) && isxdigit(p[*ip + 1])) { *ip += 2; return hexv(p[*ip - 2]) * 16 + hexv(p[*ip - 1]); }
              return 'x';
    default: return c;                             /* \. \\ \] \1 ... handled by caller */
  }
}

/* [class] -> Lua set. Range endpoints are emitted raw (Lua: "x-y" only if x is not
 * '%'-escaped), everything else '%'-escaped. */
static int xclass(const unsigned char *p, int *ip, int ci) {
  int i = *ip + 1, first = 1, c, cls;
  emit('[');
  if (p[i] == '^') { emit('^'); i++; }
  for (;; first = 0) {
    if (!p[i]) return fail("unterminated-class");
    if (p[i] == ']' && !first) { i++; break; }
    if (p[i] == '\\') {
      c = escape(p, &i, &cls);
      if (cls) {                                   /* \d \w \s \D \W \S inside a set */
        emit('%'); emit(cls == 'w' ? 'w' : cls);
        if (cls == 'w') emit('_');                 /* Lua %w lacks '_' ; %W (not-alnum) still includes '_' -> documented deviation */
        continue;
      }
    } else c = p[i++];
    if (p[i] == '-' && p[i + 1] && p[i + 1] != ']') {       /* range c-e */
      int e, ecls; i++;
      if (p[i] == '\\') { e = escape(p, &i, &ecls); if (ecls) return fail("class-escape-as-range-end"); }
      else e = p[i++];
      if (c == '%' || c == ']' || e == '%' || e == ']') return fail("range-endpoint-%-or-]");
      emit(c); emit('-'); emit(e);
      if (ci && isalpha(c) && isalpha(e) && islower(c) == islower(e)) {
        emit(c ^ 0x20); emit('-'); emit(e ^ 0x20);
      }
    } else lit(c, 1, ci);
  }
  emit(']');
  *ip = i;
  return 0;
}

/* whole regex -> Lua pattern; returns -1 (why set) if not expressible */
static int xlate(const unsigned char *p, int ci) {
  int i = 0, len = (int)strlen((const char *)p);
  int item = -1;               /* out index where the last quantifiable single item starts, -1 = none */
  int quant = 0;               /* last item already carries a quantifier */
  char stk[256]; int sp = 0;   /* group stack: 'c' capturing, 'n' (?: */
  n = 0;
  while (i < len) {
    int c = p[i], c2 = p[i + 1], start = n, cls;
    switch (c) {
      case '^':
        if (n == 0) { emit('^'); i++; item = -1; continue; }
        return fail("anchor-^-not-at-start");
      case '$': {
        int j = i + 1, k = sp;                     /* ok if only (?: closers follow */
        while (p[j] == ')' && k > 0 && stk[k - 1] == 'n') { j++; k--; }
        if (j != len) return fail("anchor-$-not-at-end");
        emit('$'); i++; item = -1; continue;
      }
      case '(':
        if (sp >= 255) return fail("nesting");
        if (c2 == '?') {
          if (p[i + 2] == ':') { stk[sp++] = 'n'; i += 3; }
          else if (p[i + 2] == '=' || p[i + 2] == '!' || p[i + 2] == '<') return fail("lookaround");
          else return fail("inline-flags");
        } else { stk[sp++] = 'c'; emit('('); i++; }
        item = -1; quant = 0; continue;
      case ')':
        if (sp == 0) return fail("unbalanced-paren");
        if (stk[--sp] == 'c') emit(')');
        i++;
        if (i < len && (p[i] == '*' || p[i] == '+' || p[i] == '?' || (p[i] == '{' && isdigit(p[i + 1]))
            || (p[i] == '{' && p[i + 1] == ',' && isdigit(p[i + 2])))) return fail("quantified-group");
        item = -1; quant = 0; continue;
      case '|': return fail("alternation");
      case '{': {
        int j = i + 1, d = 0;
        while (isdigit(p[j])) { j++; d++; }
        if (p[j] == ',') { j++; while (isdigit(p[j])) { j++; d++; } }
        if (p[j] == '}' && d > 0) return fail("counted-{n,m}");
        lit('{', 0, ci); i++; item = start; quant = 0; continue;   /* literal '{' */
      }
      case '*': case '+': case '?':
        if (item < 0) return fail("nothing-to-repeat");
        if (quant) return fail("multiple-repeat");
        if (c2 == '?') {                           /* lazy */
          if (c == '*') emit('-');
          else if (c == '+') { int e = n, k; for (k = item; k < e; k++) emit(out[k]); emit('-'); }
          else return fail("lazy-??");
          i += 2;
        } else { emit(c); i++; }
        quant = 1; continue;
      case '.': emit('.'); i++; item = start; quant = 0; continue;
      case '[':
        if (xclass(p, &i, ci) < 0) return -1;
        item = start; quant = 0; continue;
      case '\\':
        if (c2 == 'b' || c2 == 'B') return fail("word-boundary");
        if (c2 == 'A' || c2 == 'Z' || c2 == 'z') return fail("\\A-\\Z-anchor");
        if (c2 >= '1' && c2 <= '9') { emit('%'); emit(c2); i += 2; item = -1; quant = 0; continue; } /* backref */
        c = escape(p, &i, &cls);
        if (cls == 'w') { emit('['); emit('%'); emit('w'); emit('_'); emit(']'); }
        else if (cls == 'W') { emit('['); emit('^'); emit('%'); emit('w'); emit('_'); emit(']'); }
        else if (cls) { emit('%'); emit(cls); }
        else lit(c, 0, ci);
        item = start; quant = 0; continue;
      default: lit(c, 0, ci); i++; item = start; quant = 0; continue;
    }
  }
  if (sp) return fail("unbalanced-paren");
  return 0;
}

static const char *tx, *pat; static size_t tl, pl;
static int run(lua_State *L) {                     /* == str_find_aux's search loop */
  MatchState ms; const char *s1 = tx, *p = pat; size_t lp = pl;
  int anchor = (*p == '^');
  if (anchor) { p++; lp--; }
  prepstate(&ms, L, tx, tl, p, lp);
  do {
    const char *e; int i;
    reprepstate(&ms);
    if ((e = match(&ms, s1, p)) != NULL) {
      for (i = 0; i < ms.level; i++)                 /* lstrlib's get_onecapture raises this */
        if (ms.capture[i].len == CAP_UNFINISHED) { puts("error lua:unfinished capture"); return 0; }
      printf("match %d %d\n", (int)(s1 - tx), (int)(e - tx));
      for (i = 0; i < ms.level; i++) {
        ptrdiff_t l = ms.capture[i].len == CAP_POSITION ? 0 : ms.capture[i].len;
        printf("%d %d %d\n", i + 1, (int)(ms.capture[i].init - tx), (int)(ms.capture[i].init - tx + l));
      }
      return 0;
    }
  } while (s1++ < ms.src_end && !anchor);
  puts("nomatch");
  return 0;
}

int main(int argc, char **argv) {
  int ci = 0, a = 1; lua_State *L;
  if (a < argc && !strcmp(argv[a], "-i")) { ci = 1; a++; }
  if (argc - a != 2) { puts("error usage"); return 2; }
  if (getenv("LUAPAT_RAW")) { n = (int)strlen(argv[a]); memcpy(out, argv[a], n); }   /* probe mode: pattern is native Lua syntax */
  else if (xlate((const unsigned char *)argv[a], ci) < 0) { printf("error unsupported:%s\n", why); return 1; }
  if (getenv("LUAPAT_DEBUG")) fprintf(stderr, "lua pattern: %.*s\n", n, out);
  pat = out; pl = (size_t)n; tx = argv[a + 1]; tl = strlen(tx);
  L = luaL_newstate();
  lua_pushcfunction(L, run);
  if (lua_pcall(L, 0, 0, 0) != LUA_OK) printf("error lua:%s\n", lua_tostring(L, -1));
  lua_close(L);
  return 0;
}
