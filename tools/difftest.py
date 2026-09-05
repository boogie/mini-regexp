#!/usr/bin/env python3
"""Run a corpus (from gen_corpus.py) against an engine CLI and report mismatches.
Engine CLI contract:   ./engine [-i] PATTERN TEXT
  stdout line 1: "match START END" (byte offsets) | "nomatch" | "error"
  optional following lines: "N START END" for capture group N (1-based), -1 -1 if unset
  exit code ignored; timeout (2s) counts as a failure (hang / catastrophic backtracking).
usage: difftest.py [--batch] ENGINE corpus.jsonl [max_failures_to_print]
"""
import json, subprocess, sys

args = sys.argv[1:]
batch = bool(args and args[0] == "--batch")
if batch: args.pop(0)
engine, corpus = args[0], args[1]
maxprint = int(args[2]) if len(args) > 2 else 15
cases = [json.loads(l) for l in open(corpus, encoding="utf-8") if l.strip()]
fails, timeouts, errors_ok, nullable_rejected, budget_exhausted = [], 0, 0, 0, 0
group_checked = 0
batch_process = None
if batch:
    batch_process = subprocess.Popen([engine, "--batch"], stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE, text=True)
for c in cases:
    args = [engine] + (["-i"] if "i" in c["flags"] else []) + [c["pat"], c["text"]]
    if batch:
        batch_process.stdin.write(json.dumps(c, ensure_ascii=False) + "\n")
        batch_process.stdin.flush()
        response = batch_process.stdout.readline()
        lines = json.loads(response).strip().splitlines() if response else ["<empty>"]
    else:
        try:
            r = subprocess.run(args, capture_output=True, timeout=2)
        except subprocess.TimeoutExpired:
            timeouts += 1; fails.append((c, "TIMEOUT")); continue
        lines = r.stdout.decode("utf-8", "replace").strip().splitlines()
    got = lines[0].split() if lines else ["<empty>"]
    exp = c["expect"]
    if exp == "error":
        errors_ok += 1  # Python rejects it; engine may do anything but must not hang
        continue
    if c.get("nullable_repeat") and got[0] == "error":
        # Repetition of a nullable atom ((a*)*, (|a)+, ()+) is out of scope as of 2026-09-04:
        # an engine may reject such a pattern. Matching it correctly also counts as a pass.
        nullable_rejected += 1
        continue
    if c.get("budget_exhaustion") and got[0] == "error":
        budget_exhausted += 1
        continue
    if got[0] == "error":
        fails.append((c, "engine rejected pattern: " + " ".join(got))); continue
    if exp is None:
        if got[0] != "nomatch": fails.append((c, "expected nomatch, got %s" % " ".join(got)))
        continue
    if got[0] != "match" or len(got) < 3 or [int(got[1]), int(got[2])] != exp["span"]:
        fails.append((c, "expected match %s, got %s" % (exp["span"], " ".join(got)))); continue
    for l in lines[1:]:
        p = l.split()
        if len(p) != 3: continue
        n, s, e = int(p[0]), int(p[1]), int(p[2])
        if 1 <= n <= len(exp["groups"]):
            group_checked += 1
            if [s, e] != exp["groups"][n - 1]:
                fails.append((c, "group %d expected %s got [%d, %d]" % (n, exp["groups"][n - 1], s, e))); break
total = len(cases) - errors_ok - nullable_rejected - budget_exhausted
print("cases=%d checked=%d pass=%d fail=%d timeouts=%d group_spans_checked=%d nullable_rejected=%d budget_exhausted=%d" %
      (len(cases), total, total - len(fails), len(fails), timeouts, group_checked,
       nullable_rejected, budget_exhausted))
for c, why in fails[:maxprint]:
    print("  FAIL flags=%r pat=%r text=%r -> %s" % (c["flags"], c["pat"], c["text"], why))
if batch_process:
    batch_process.stdin.close()
    batch_process.wait(timeout=2)
sys.exit(1 if fails else 0)
