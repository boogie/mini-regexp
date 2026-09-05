#!/usr/bin/env node
"use strict";

const {compileRe} = require("../../../tools/re_compile.js");
const {transform} = require("../bytecode-backends/backend.js");
const variants = {
  baseline: {}, char: {char: true}, short: {short: true}, range: {range: true},
  star: {char: true, star: true}, tail: {tail: true},
  char_escape_tail: {char: true, short: true, escape: true, tail: true},
};
const samples = [["abc", false], ["[a-z]+", true], ["a*", true], ["\\bfoo\\b", true]];

console.log(["pattern", "mode", ...Object.keys(variants)].join("\t"));
for (const [pattern, search] of samples) {
  const bytes = compileRe(pattern, {search});
  console.log([pattern, search ? "search" : "prefix",
    ...Object.values(variants).map(options => transform(bytes, options).length)].join("\t"));
}
