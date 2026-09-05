#!/usr/bin/env node
"use strict";

const MATCH = 0;
const BOL = 1;
const EOL = 2;
const WORD_BOUNDARY = 3;
const LINE_BOL = 4;
const LINE_EOL = 5;
const CLASS = 6;
const SPLIT = 7;
const JUMP = 8;
const NOT_WORD_BOUNDARY = 9;
const SAVE = 10;

const BITS_PER_BYTE = 8;
const BYTE_MAX = 0xff;
const OFFSET_MASK = 0xffff;
const OFFSET_MIN = -0x8000;
const OFFSET_MAX = 0x7fff;
const OFFSET_BYTES = 2;
const OPCODE_BYTES = 1;
const BRANCH_BYTES = OPCODE_BYTES + OFFSET_BYTES;
const OFFSET_PLACEHOLDER = [0, 0];
const CLASS_NEGATED = 0x80;
const CLASS_RANGE_MASK = 0x7f;
const MAX_CLASS_RANGES = CLASS_RANGE_MASK;
const MAX_UNICODE_CODE_POINT = 0x10ffff;
const SURROGATE_MIN = 0xd800;
const SURROGATE_MAX = 0xdfff;
const MAX_FOLD_EXPANSION_INPUT = 4096;
const CAPTURE_PAIR_SLOTS = 2;
const CAPTURE_END_OFFSET = 1;
const OVERALL_CAPTURE_PAIRS = 1;
const OVERALL_CAPTURE_START = 0;
const OVERALL_CAPTURE_END = 1;
const MAX_CAPTURE_SLOTS = BYTE_MAX;
const codePointOf = (character) => character.codePointAt(0);
const LINE_FEED = codePointOf("\n");
const ASCII_RANGES = Object.freeze({
  d: [[codePointOf("0"), codePointOf("9")]],
  w: [[codePointOf("0"), codePointOf("9")], [codePointOf("A"), codePointOf("Z")],
    [codePointOf("_"), codePointOf("_")], [codePointOf("a"), codePointOf("z")]],
  s: [[codePointOf("\t"), codePointOf("\r")], [codePointOf(" "), codePointOf(" ")]],
});
const CONTROL_ESCAPES = Object.freeze({n: "\n", r: "\r", t: "\t", f: "\f", v: "\v"});

function encodeCodePoint(codePoint) {
  return [...Buffer.from(String.fromCodePoint(codePoint), "utf8")];
}

function offset(value) {
  if (value < OFFSET_MIN || value > OFFSET_MAX) {
    throw new Error("jump out of word range");
  }
  const encoded = value & OFFSET_MASK;
  return [encoded & BYTE_MAX, encoded >>> BITS_PER_BYTE];
}

function setOffset(bytes, index, value) {
  const encoded = offset(value);
  bytes[index] = encoded[0];
  bytes[index + 1] = encoded[1];
}

function merge(ranges) {
  if (ranges.length === 0) return [];
  const sorted = ranges.map((range) => [...range]).sort((left, right) => left[0] - right[0]);
  const output = [sorted[0]];
  for (const [lower, upper] of sorted.slice(1)) {
    const previous = output[output.length - 1];
    if (lower <= previous[1] + 1) previous[1] = Math.max(previous[1], upper);
    else output.push([lower, upper]);
  }
  return output;
}

function complement(ranges) {
  const output = [];
  let position = 0;
  for (const [lower, upper] of merge(ranges)) {
    if (position < lower) output.push([position, lower - 1]);
    position = upper + 1;
  }
  if (position <= MAX_UNICODE_CODE_POINT) output.push([position, MAX_UNICODE_CODE_POINT]);
  return output;
}

let foldReverse;

function oneCodePoint(value) {
  return [...value].length === 1;
}

function caseEquivalent(codePoint, candidate) {
  const expression = new RegExp(`^\\u{${codePoint.toString(16)}}$`, "iu");
  return expression.test(candidate);
}

function foldKey(codePoint) {
  const character = String.fromCodePoint(codePoint);
  const lower = character.toLowerCase();
  if (lower !== character && oneCodePoint(lower) && caseEquivalent(codePoint, lower)) {
    return lower.codePointAt(0);
  }

  const upper = character.toUpperCase();
  if (upper !== character && oneCodePoint(upper)) {
    const loweredUpper = upper.toLowerCase();
    if (oneCodePoint(loweredUpper) && caseEquivalent(codePoint, loweredUpper)) {
      return loweredUpper.codePointAt(0);
    }
  }
  return codePoint;
}

function foldMap() {
  if (foldReverse) return foldReverse;
  foldReverse = new Map();
  for (let codePoint = 0; codePoint <= MAX_UNICODE_CODE_POINT; codePoint++) {
    if (codePoint >= SURROGATE_MIN && codePoint <= SURROGATE_MAX) continue;
    const key = foldKey(codePoint);
    if (key !== codePoint) {
      const reverse = foldReverse.get(key) || [];
      reverse.push(codePoint);
      foldReverse.set(key, reverse);
    }
  }
  return foldReverse;
}

function unicodeFold(ranges) {
  const total = ranges.reduce((sum, [lower, upper]) => sum + upper - lower + 1, 0);
  if (total > MAX_FOLD_EXPANSION_INPUT) throw new Error("case-insensitive class too wide to expand");

  const reverse = foldMap();
  const output = [];
  for (const [lower, upper] of ranges) {
    for (let codePoint = lower; codePoint <= upper; codePoint++) {
      const key = foldKey(codePoint);
      output.push([codePoint, codePoint], [key, key]);
      for (const equivalent of reverse.get(key) || []) output.push([equivalent, equivalent]);
    }
  }
  return merge(output);
}

function classBytes(ranges, negated = false) {
  const normalized = merge(ranges);
  if (normalized.length > MAX_CLASS_RANGES) throw new Error("too many class ranges");
  const output = [CLASS, (negated ? CLASS_NEGATED : 0) | normalized.length];
  for (const [lower, upper] of normalized) {
    output.push(...encodeCodePoint(lower), ...encodeCodePoint(upper));
  }
  return output;
}

class Parser {
  constructor(source, {ignoreCase = false, multiline = false, dotAll = false} = {}) {
    this.source = source;
    this.index = 0;
    this.ignoreCase = ignoreCase;
    this.multiline = multiline;
    this.dotAll = dotAll;
    this.captureCount = 0;
  }

  peek() {
    if (this.index >= this.source.length) return "";
    return String.fromCodePoint(this.source.codePointAt(this.index));
  }

  get() {
    const character = this.peek();
    this.index += character.length;
    return character;
  }

  expression(stop = "") {
    const leftFragment = this.sequence(`${stop}|`);
    if (this.peek() !== "|") return leftFragment;

    this.get();
    const rightFragment = this.expression(stop);
    const left = [...leftFragment.bytes, JUMP, ...OFFSET_PLACEHOLDER];
    const output = [SPLIT, ...OFFSET_PLACEHOLDER, ...left, ...rightFragment.bytes];
    setOffset(output, OPCODE_BYTES, left.length);
    const jumpIndex = BRANCH_BYTES + leftFragment.bytes.length + OPCODE_BYTES;
    setOffset(output, jumpIndex, rightFragment.bytes.length);
    return {bytes: output, nullable: leftFragment.nullable || rightFragment.nullable};
  }

  sequence(stop) {
    const bytes = [];
    let nullable = true;
    while (this.peek() && !stop.includes(this.peek())) {
      const fragment = this.quantified(this.atom());
      bytes.push(...fragment.bytes);
      nullable = nullable && fragment.nullable;
    }
    return {bytes, nullable};
  }

  literal(character) {
    let ranges = [[character.codePointAt(0), character.codePointAt(0)]];
    if (this.ignoreCase) ranges = unicodeFold(ranges);
    return {bytes: classBytes(ranges), nullable: false};
  }

  atom() {
    const character = this.get();
    if (character === ".") {
      return {bytes: this.dotAll ? classBytes([], true) : classBytes([[LINE_FEED, LINE_FEED]], true), nullable: false};
    }
    if (character === "^") return {bytes: [this.multiline ? LINE_BOL : BOL], nullable: true};
    if (character === "$") return {bytes: [this.multiline ? LINE_EOL : EOL], nullable: true};
    if (character === "(") {
      const capturing = !this.source.startsWith("?:", this.index);
      if (!capturing) this.index += 2;
      const group = capturing ? ++this.captureCount : 0;
      const fragment = this.expression(")");
      if (this.get() !== ")") throw new Error("missing )");
      if (capturing) {
        return {bytes: [SAVE, group * CAPTURE_PAIR_SLOTS, ...fragment.bytes,
          SAVE, group * CAPTURE_PAIR_SLOTS + CAPTURE_END_OFFSET],
                nullable: fragment.nullable};
      }
      return fragment;
    }
    if (character === "[") return this.characterClass();
    if (character === "\\") {
      const escaped = this.get();
      if (escaped === "b") return {bytes: [WORD_BOUNDARY], nullable: true};
      if (escaped === "B") return {bytes: [NOT_WORD_BOUNDARY], nullable: true};
      if (escaped === "A") return {bytes: [BOL], nullable: true};
      if (escaped === "z") return {bytes: [EOL], nullable: true};
      if (Object.hasOwn(CONTROL_ESCAPES, escaped)) return this.literal(CONTROL_ESCAPES[escaped]);
      if ("dDwWsS".includes(escaped)) {
        let ranges = ASCII_RANGES[escaped.toLowerCase()];
        if (this.ignoreCase) ranges = unicodeFold(ranges);
        return {bytes: classBytes(ranges, escaped === escaped.toUpperCase()), nullable: false};
      }
      return this.literal(escaped);
    }
    if (!character) throw new Error("atom expected");
    return this.literal(character);
  }

  classAtom() {
    if (this.peek() === "\\") {
      this.get();
      const escaped = this.get();
      if ("dDwsWS".includes(escaped)) {
        const base = ASCII_RANGES[escaped.toLowerCase()];
        return escaped === escaped.toUpperCase() ? complement(base) : base;
      }
      return [[escaped.codePointAt(0), escaped.codePointAt(0)]];
    }
    const character = this.get();
    return [[character.codePointAt(0), character.codePointAt(0)]];
  }

  characterClass() {
    const negated = this.peek() === "^";
    if (negated) this.get();
    let ranges = [];
    while (this.peek() && this.peek() !== "]") {
      const first = this.classAtom();
      if (first.length === 1 && this.peek() === "-" &&
          this.index + 1 < this.source.length && this.source[this.index + 1] !== "]") {
        this.get();
        const second = this.classAtom();
        if (second.length !== 1) throw new Error("class range endpoint must be literal");
        ranges.push([first[0][0], second[0][0]]);
      } else {
        ranges.push(...first);
      }
    }
    if (this.get() !== "]") throw new Error("missing ]");
    ranges = merge(ranges);
    if (this.ignoreCase) ranges = unicodeFold(ranges);
    return {bytes: classBytes(ranges, negated), nullable: false};
  }

  quantified(fragment) {
    const character = this.peek();
    if (!character || !"*+?{".includes(character)) return fragment;

    let lower;
    let upper;
    if (character === "*") {
      this.get(); lower = 0; upper = null;
    } else if (character === "+") {
      this.get(); lower = 1; upper = null;
    } else if (character === "?") {
      this.get(); lower = 0; upper = 1;
    } else {
      this.get(); lower = this.number(); upper = lower;
      if (this.peek() === ",") {
        this.get(); upper = this.peek() === "}" ? null : this.number();
      }
      if (this.get() !== "}") throw new Error("missing }");
      if (upper !== null && upper < lower) throw new Error("bad repeat");
    }

    const lazy = this.peek() === "?";
    if (lazy) this.get();
    const bytes = [];
    for (let count = 0; count < lower; count++) bytes.push(...fragment.bytes);
    if (upper !== null) {
      for (let count = lower; count < upper; count++) bytes.push(...optional(fragment.bytes, lazy));
      return {bytes, nullable: lower === 0 || fragment.nullable};
    }
    if (fragment.nullable) throw new Error("unbounded repeat of nullable atom");
    bytes.push(...star(fragment.bytes, lazy));
    return {bytes, nullable: lower === 0};
  }

  number() {
    const start = this.index;
    while (/^[0-9]$/.test(this.peek())) this.index += 1;
    if (start === this.index) throw new Error("number expected");
    return Number(this.source.slice(start, this.index));
  }
}

function optional(bytes, lazy = false) {
  if (!lazy) return [SPLIT, ...offset(bytes.length), ...bytes];
  return [SPLIT, ...offset(BRANCH_BYTES), JUMP, ...offset(bytes.length), ...bytes];
}

function star(bytes, lazy = false) {
  if (!lazy) {
    const output = [SPLIT, ...OFFSET_PLACEHOLDER, ...bytes, JUMP, ...OFFSET_PLACEHOLDER];
    setOffset(output, OPCODE_BYTES, bytes.length + BRANCH_BYTES);
    setOffset(output, output.length - OFFSET_BYTES, -output.length);
    return output;
  }
  const output = [SPLIT, ...offset(BRANCH_BYTES), JUMP, ...OFFSET_PLACEHOLDER,
    ...bytes, JUMP, ...OFFSET_PLACEHOLDER];
  setOffset(output, BRANCH_BYTES + OPCODE_BYTES, bytes.length + BRANCH_BYTES);
  setOffset(output, output.length - OFFSET_BYTES, -output.length);
  return output;
}

function compileRe(source, options = {}) {
  const parser = new Parser(source, options);
  const fragment = parser.expression();
  if (parser.index !== source.length) throw new Error(`unexpected ${parser.peek()}`);
  const slots = (parser.captureCount + OVERALL_CAPTURE_PAIRS) * CAPTURE_PAIR_SLOTS;
  if (slots > MAX_CAPTURE_SLOTS) throw new Error("too many capture groups");
  const core = [SAVE, OVERALL_CAPTURE_START, ...fragment.bytes, SAVE, OVERALL_CAPTURE_END, MATCH];
  if (options.search === false) return Uint8Array.from([slots, ...core]);

  const output = [SPLIT, ...OFFSET_PLACEHOLDER, ...core, ...classBytes([], true),
    JUMP, ...OFFSET_PLACEHOLDER];
  setOffset(output, OPCODE_BYTES, core.length);
  setOffset(output, output.length - OFFSET_BYTES, -output.length);
  return Uint8Array.from([slots, ...output]);
}

function emit(name, source, options = {}) {
  const bytes = compileRe(source, options);
  console.log(`enum { ${name}_capture_slots = ${bytes[0]} };`);
  console.log(`static const unsigned char ${name}[] = {${[...bytes].join(",")}};`);
  const flags = `${options.ignoreCase ? " /i" : ""}${options.multiline ? " /m" : ""}${options.dotAll ? " /s" : ""}`;
  console.log(`/* ${JSON.stringify(source)}${flags} : ${bytes.length} bytes */`);
}

function main(argv) {
  const args = [...argv];
  const options = {};
  while (args.length > 0 && args[0].startsWith("-")) {
    const flags = args.shift();
    options.ignoreCase ||= flags.includes("i");
    options.multiline ||= flags.includes("m");
    options.dotAll ||= flags.includes("s");
  }
  if (args.length === 0) {
    console.error("usage: tinyre_compile_opt.js [-ims] REGEX [NAME]");
    return 2;
  }
  emit(args[1] || "re", args[0], options);
  return 0;
}

module.exports = {compileRe, emit};

if (require.main === module) process.exitCode = main(process.argv.slice(2));
