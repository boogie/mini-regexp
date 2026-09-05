#!/usr/bin/env node
"use strict";

const fs = require("node:fs");
const {hasNullableRepeat} = require("./nullable_repeat.js");

const budgetPatterns = new Set([
  "^(a+)+$",
  "(x+x+)+y",
]);

function annotate(line) {
  const test = JSON.parse(line);
  const additions = [];

  const nullableRepeat = hasNullableRepeat(test.pat);
  if (test.nullable_repeat && !nullableRepeat) {
    throw new Error(`stale nullable_repeat annotation: ${test.pat}`);
  }
  if (nullableRepeat && !test.nullable_repeat) {
    additions.push('"nullable_repeat": true');
  }

  if (budgetPatterns.has(test.pat) && !test.budget_exhaustion) {
    additions.push('"budget_exhaustion": true');
  }

  if (test.flags === "i" && test.pat === "é" && test.text === "É") {
    test.expect = {span: [0, 2], groups: []};
    let output = JSON.stringify(test);
    if (additions.length) output = output.slice(0, -1) + "," + additions.join(",") + "}";
    return output;
  }

  if (!additions.length) return line;
  return line.slice(0, -1) + ", " + additions.join(", ") + "}";
}

for (const file of process.argv.slice(2)) {
  const input = fs.readFileSync(file, "utf8");
  const trailingNewline = input.endsWith("\n") ? "\n" : "";
  const output = input.trimEnd().split("\n").filter(Boolean).map(annotate).join("\n") + trailingNewline;
  fs.writeFileSync(file, output);
}
