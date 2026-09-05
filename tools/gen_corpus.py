#!/usr/bin/env python3
"""Generate a differential-test corpus for a Perl/JS-semantics regex engine.
Oracle: Python `re` with re.ASCII (so \\d \\w \\s are ASCII-only; `.` and classes
still operate on full Unicode code points -> UTF-8 aware engine expected).
Output: JSON lines {flags, pat, text, expect}; expect = null (no match) or
{"span":[start,end], "groups":[[s,e],...]} with BYTE offsets into UTF-8 text.
usage: gen_corpus.py [seed] [count] > corpus.jsonl
"""
import json, random, re, sys

WB = "--wb" in sys.argv          # include \\b / \\B (v1 as of 2026-09-04)
sys.argv = [a for a in sys.argv if a != "--wb"]
seed = int(sys.argv[1]) if len(sys.argv) > 1 else 1
count = int(sys.argv[2]) if len(sys.argv) > 2 else 2000
rng = random.Random(seed)

ASCII = list("abcxyz019_ -")
MULTI = ["é", "ő", "ű", "日", "本", "😀", "ß"]
META = list(".*+?()[]{}|^$\\")

def lit():
    r = rng.random()
    if r < 0.75: return rng.choice(ASCII)
    if r < 0.9:  return rng.choice(MULTI)
    return "\\" + rng.choice(META)          # escaped metachar

def cls():
    items = []
    for _ in range(rng.randint(1, 3)):
        r = rng.random()
        if r < 0.4:
            a = rng.choice("abcxyz019")
            b = chr(ord(a) + rng.randint(1, 5))
            items.append(a + "-" + b)
        elif r < 0.6: items.append(rng.choice(["\\d", "\\w", "\\s"]))
        elif r < 0.75: items.append(rng.choice(MULTI))
        elif r < 0.85: items.append("\\" + rng.choice("]\\-^"))
        else: items.append(rng.choice(ASCII).replace("\\", "\\\\"))
    neg = "^" if rng.random() < 0.3 else ""
    return "[" + neg + "".join(items) + "]"

def atom(depth):
    r = rng.random()
    if r < 0.45: return lit()
    if r < 0.55: return "."
    if r < 0.66: return rng.choice(["\\d", "\\w", "\\s", "\\D", "\\W", "\\S"])
    if r < 0.70: return rng.choice(["\\b", "\\B"]) + "" if WB else lit()
    if r < 0.82: return cls()
    if depth < 2:
        open_ = "(" if rng.random() < 0.6 else "(?:"
        return open_ + alt(depth + 1) + ")"
    return lit()

def quant():
    r = rng.random()
    if r < 0.55: q = ""
    elif r < 0.7: q = "*"
    elif r < 0.8: q = "+"
    elif r < 0.88: q = "?"
    else:
        n = rng.randint(0, 3)
        q = rng.choice(["{%d}" % n, "{%d,}" % n, "{%d,%d}" % (n, n + rng.randint(0, 2))])
    if q and rng.random() < 0.25: q += "?"     # lazy
    return q

def seq(depth):
    parts = []
    for _ in range(rng.randint(0, 4)):
        parts.append(atom(depth) + quant())
    return "".join(parts)

def alt(depth):
    n = 1 if rng.random() < 0.65 else rng.randint(2, 3)
    return "|".join(seq(depth) for _ in range(n))

def pattern():
    p = alt(0)
    if rng.random() < 0.15: p = "^" + p
    if rng.random() < 0.15: p = p + "$"
    return p

def text_for(pat):
    pool = [c for c in pat if c not in META] + ASCII + MULTI
    return "".join(rng.choice(pool) for _ in range(rng.randint(0, 12)))

def expect(pat, text, flags):
    f = re.ASCII | (re.IGNORECASE if "i" in flags else 0)
    try:
        m = re.compile(pat, f).search(text)
    except re.error:
        return "error"
    if not m: return None
    b = lambda i: len(text[:i].encode()) if i >= 0 else -1
    groups = []
    for g in range(1, m.re.groups + 1):
        s, e = m.span(g)
        groups.append([b(s), b(e)] if s >= 0 else [-1, -1])
    return {"span": [b(m.start()), b(m.end())], "groups": groups}

HAND = [  # hand-written edge cases: (flags, pat, text)
    ("", "", ""), ("", "", "abc"), ("", "a", ""), ("", "^$", ""), ("", "^", "abc"), ("", "$", "abc"),
    ("", "abc", "xabcx"), ("", "a.c", "abc"), ("", "a.c", "a\nc"), ("", "a*", "aaa"), ("", "a*", "bbb"),
    ("", "a+", "baaa"), ("", "a?b", "ab"), ("", "a??b", "ab"), ("", "a*?", "aaa"), ("", "a+?", "aaa"),
    ("", "a{2}", "aaaa"), ("", "a{2,}", "aaaa"), ("", "a{2,3}", "aaaa"), ("", "a{2,3}?", "aaaa"), ("", "a{0}", "aaa"),
    ("", "(a|ab)(c|bcd)(d*)", "abcd"), ("", "(a*)*", "b"), ("", "(a*)+", "b"), ("", "(a|b)*c", "ababc"),
    ("", "(?:ab)+", "ababab"), ("", "a|b|c", "xxc"), ("", "(a)|(b)", "b"), ("", "(a)|(b)", "a"),
    ("", "x(a|)y", "xy"), ("", "(|a)+", "aaa"), ("", "[abc]+", "xabcabx"), ("", "[^abc]+", "abxyzab"),
    ("", "[a-c]+", "abcd"), ("", "[a-]+", "a-b"), ("", "[-a]+", "-a-"), ("", "[\\]]", "a]b"), ("", "[\\\\]", "a\\b"),
    ("", "[\\d]+", "ab12cd"), ("", "[^\\d]+", "12ab34"), ("", "\\d+", "abc123def"), ("", "\\w+", "!!héllo_1!!"),
    ("", "\\s+", "a \t b"), ("", "\\D\\W\\S", "1 a"), ("", "\\.", "a.b"), ("", "\\*", "a*b"), ("", "\\\\", "a\\b"),
    ("", "\\(\\)", "()"), ("", "a\\|b", "a|b"), ("", "\\^\\$", "^$"), ("", "\\{2\\}", "{2}"),
    ("", ".", "é"), ("", ".", "😀"), ("", "^.$", "日"), ("", "^..$", "日本"), ("", "é+", "ééé"), ("", "[é日]+", "aé日b"),
    ("", "[^é]+", "ééabcé"), ("", "^[^a]$", "😀"), ("", "^.{2}$", "éő"), ("", "(.)(.)", "éő"), ("", "😀*", "😀😀x"),
    ("", "[a-zé]+", "xyzéa1"), ("", "^\\w+$", "héllo"), ("", "ő?ű", "ű"), ("", "ő?ű", "őű"),
    ("i", "abc", "xABCx"), ("i", "[a-c]+", "ABC"), ("i", "[^a-c]+", "ABCd"), ("i", "\\w+", "ÁRVÍZ"), ("i", "é", "É"),
    ("", "abc$", "abcd"), ("", "a$", "a\nb"), ("", "^b", "a\nb"), ("", ".*", "ab\ncd"),
    ("", "(a)(b)?(c)", "ac"), ("", "(a(b(c)))", "abc"), ("", "((a)|(b))+", "ab"), ("", "(a*)(b*)(c*)", "cba"),
    ("", "^(a+)+$", "aaaaaaaaaaaaaaaaaaaaaaaaaaab"),  # catastrophic pattern, must still terminate
    ("", "(x+x+)+y", "xxxxxxxxxxxxxxxxxxxx"),
    ("", "\\bfoo\\b", "a foo b"), ("", "\\bfoo\\b", "afoob"), ("", "\\b", "  a"), ("", "\\b", "   "), ("", "\\B", "ab"),
    ("", "\\Bo\\B", "foo"), ("", "^\\b", "a"), ("", "\\b$", "a"), ("", "\\b\\w+\\b", "  héllo  "), ("", "\\bé", "aé"),
    ("", "\\b.", "é"), ("", "a\\b", "a_"), ("", "\\b\\d+\\b", "x 42 y"), ("", "(\\b\\w)+", "ab cd"), ("", "\\b*", "a"),
    ("", "a{1,2}{2}", "aaaa"),   # error in Python -> engine may reject or accept; reported as 'error'
    ("", "a**", "aa"), ("", "(", "a"), ("", "[a", "a"), ("", "a{2,1}", "aa"), ("", "*a", "a"), ("", "a|*", "a"),
]

out = []
for flags, pat, text in HAND:
    out.append({"flags": flags, "pat": pat, "text": text, "expect": expect(pat, text, flags), "hand": True})
n = 0
while n < count:
    pat = pattern()
    text = text_for(pat)
    flags = "i" if rng.random() < 0.1 else ""
    e = expect(pat, text, flags)
    if e == "error": continue
    out.append({"flags": flags, "pat": pat, "text": text, "expect": e})
    n += 1
for o in out:
    print(json.dumps(o, ensure_ascii=False))
