#!/usr/bin/env node
"use strict";

const assert = require("node:assert/strict");
const {hasNullableRepeat} = require("./nullable_repeat.js");

for (const pattern of ["(a*)*", "(|a)+", "\\b*", "(?:a?){2,}", "(^)+", "(-*()😀?)+"]) {
  assert.equal(hasNullableRepeat(pattern), true, pattern);
}
for (const pattern of ["a*", "(ab)+", "(?:a?){2}", "[a*]+", "\\**", "(a|b)+"]) {
  assert.equal(hasNullableRepeat(pattern), false, pattern);
}

console.log("ok");
