#!/usr/bin/env node
"use strict";

const assert = require("node:assert/strict");
const {spawnSync} = require("node:child_process");
const {compileRe} = require("../../../tools/re_compile.js");

const compiler = process.env.TINYRE_C_COMPILER;
if (!compiler) throw new Error("set TINYRE_C_COMPILER");

/* The C compiler emits the unshared format, so parity is measured against the JavaScript
 * compiler with tail sharing disabled. Tail sharing is a JavaScript-side post-pass; see
 * docs/feasibility.md section 6.7. */
const NO_TAIL = {tail: false};

const patterns = [
  "", "a", "abc", ".", "^.$", "a|b|c", "(a)|(b)", "(?:ab)+", "a*?", "a{2,3}?",
  "[a-z]+", "[^a]+", "[\\d]+", "\\D\\W\\S", "\\bfoo\\b", "é+", "😀*",
  "[é-ê]+", "(a)(b)?(c)", "^(a+)+$", "x(a|)y", "a{0}", "a{2,}", ".*",
];

for (const pattern of patterns) {
  const result = spawnSync(compiler, [pattern], {encoding: "utf8"});
  assert.equal(result.status, 0, `${pattern}: ${result.stderr}`);
  const expected = Buffer.from(compileRe(pattern, NO_TAIL)).toString("hex");
  assert.equal(result.stdout.trim(), expected, pattern);
}

for (const [pattern, flags] of [["a+", "a"], ["^.$", "m"], [".", "s"], ["^.$", "ams"]]) {
  const result = spawnSync(compiler, [pattern, flags], {encoding: "utf8"});
  const expected = Buffer.from(compileRe(pattern, {
    ...NO_TAIL,
    search: !flags.includes("a"), multiline: flags.includes("m"), dotAll: flags.includes("s"),
  })).toString("hex");
  assert.equal(result.stdout.trim(), expected, `${pattern}/${flags}`);
}

console.log("ok");
