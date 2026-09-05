#!/usr/bin/env node
"use strict";

const assert = require("node:assert/strict");
const path = require("node:path");
const {spawnSync} = require("node:child_process");

const adapter = path.join(__dirname, "corpus_adapter.js");
const helper = process.env.TINYRE_VM_ADAPTER;

if (!helper) throw new Error("set TINYRE_VM_ADAPTER to the compiled C adapter");

function match(pattern, text, flags = []) {
  const result = spawnSync(adapter, [...flags, pattern, text], {
    encoding: "utf8",
    env: {...process.env, TINYRE_VM_ADAPTER: helper},
  });
  assert.equal(result.status, 0, result.stderr);
  return result.stdout.trim();
}

assert.equal(match("a+", "baaa"), "match 1 4");
assert.equal(match("a+?", "baaa"), "match 1 2");
assert.equal(match("a{2,3}?", "aaaa"), "match 0 2");
assert.equal(match("(?:ab)+", "abab"), "match 0 4");
assert.equal(match(".", "\n"), "nomatch");
assert.equal(match("^.$", "é"), "match 0 2");
assert.equal(match("a$", "a\n"), "nomatch");
assert.equal(match("\\d+", "x12"), "match 1 3");
assert.equal(match("\\w+", "éa"), "match 2 3");
assert.equal(match("[é-ê]+", "ê"), "match 0 2");
assert.equal(match("[^a]+", "aé"), "match 1 3");
assert.equal(match("\\B", "ab"), "match 1 1");
assert.equal(match("é+", "Éé", ["-i"]), "match 0 4");
assert.equal(match("[a-z]+", "K", ["-i"]), "match 0 3");
assert.equal(match("s", "ſ", ["-i"]), "match 0 2");
assert.equal(match("σ", "ς", ["-i"]), "match 0 2");
assert.equal(match("ß", "ẞ", ["-i"]), "match 0 3");
assert.equal(match("ß", "SS", ["-i"]), "nomatch");
assert.equal(match("(a)|(b)", "b"), "match 0 1\n1 -1 -1\n2 0 1");
assert.equal(match("(a)+", "aaa"), "match 0 3\n1 2 3");
assert.equal(match("(a)(b)?(c)", "ac"), "match 0 2\n1 0 1\n2 -1 -1\n3 1 2");
assert.equal(match(".*", "x".repeat(1024)), "match 0 1024");
assert.equal(match("(a*)*", "b"), "error");

console.log("ok");
