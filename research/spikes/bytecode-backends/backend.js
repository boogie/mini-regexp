"use strict";

const assert = require("node:assert/strict");
const {compileRe} = require("./snapshot/tools/re_compile.js");

const OP = Object.freeze({
  MATCH: 0, BOL: 1, EOL: 2, WORD: 3, LINE_BOL: 4, LINE_EOL: 5, CLASS: 6,
  SPLIT: 7, JUMP: 8, NOT_WORD: 9, SAVE: 10, CHAR: 11, SPLIT_SHORT: 12,
  JUMP_SHORT: 13, CHAR_STAR: 14, ASCII_RANGE: 15,
});
const FORMAT = Object.freeze({HEADER: 1, BYTE_BITS: 8, BYTE_MASK: 255, SHORT_MIN: -128,
  SHORT_MAX: 127, LONG_MIN: -32768, LONG_MAX: 32767, CLASS_NEGATED: 128,
  CLASS_COUNT_MASK: 127, ASCII_LIMIT: 128, LONG_BRANCH_BYTES: 3, SHORT_BRANCH_BYTES: 2,
  ESCAPE: 128, ESCAPED_LONG_BYTES: 4});

function valueWidth(value) {
  return value < FORMAT.ASCII_LIMIT ? 1 : value < 224 ? 2 : value < 240 ? 3 : 4;
}

function decode(bytes) {
  const nodes = [];
  let position = FORMAT.HEADER;
  while (position < bytes.length) {
    const id = position, op = bytes[position++];
    let target = null;
    if (op === OP.CLASS) {
      const count = bytes[position++] & FORMAT.CLASS_COUNT_MASK;
      for (let bound = 0; bound < count * 2; bound++) position += valueWidth(bytes[position]);
    } else if (op === OP.SPLIT || op === OP.JUMP) {
      const unsigned = bytes[position] | bytes[position + 1] << FORMAT.BYTE_BITS;
      const offset = unsigned > FORMAT.LONG_MAX ? unsigned - 65536 : unsigned;
      position += 2;
      target = position + offset;
    } else if (op === OP.SAVE) position++;
    else assert(op >= OP.MATCH && op <= OP.NOT_WORD, `unknown opcode ${op}`);
    assert(position <= bytes.length, "truncated instruction");
    const args = op === OP.SPLIT || op === OP.JUMP ? [] : [...bytes.slice(id + 1, position)];
    nodes.push({id, op, args, target, next: op === OP.MATCH || op === OP.JUMP ? null : position});
  }
  const ids = new Set(nodes.map(node => node.id));
  for (const node of nodes) {
    assert(node.next === null || ids.has(node.next), "bad fallthrough");
    assert(node.target === null || ids.has(node.target), "bad jump");
  }
  return {slots: bytes[0], entry: FORMAT.HEADER, nodes};
}

function clone(graph) {
  return {...graph, nodes: graph.nodes.map(node => ({...node, args: [...node.args]}))};
}

function compactAtoms(graph, options) {
  for (const node of graph.nodes) {
    if (node.op !== OP.CLASS || node.args.length !== 3 || node.args[0] !== 1) continue;
    const [, lower, upper] = node.args;
    if (upper >= FORMAT.ASCII_LIMIT || lower === 0) continue;
    if (options.char && lower === upper) {
      node.op = OP.CHAR;
      node.args = [lower];
    } else if (options.range) {
      node.op = OP.ASCII_RANGE;
      node.args = [lower, upper];
    }
  }
}

function fuseStars(graph) {
  const byId = new Map(graph.nodes.map(node => [node.id, node]));
  const incoming = new Map(graph.nodes.map(node => [node.id, 0]));
  for (const node of graph.nodes) {
    for (const edge of [node.next, node.target]) if (edge !== null) incoming.set(edge, incoming.get(edge) + 1);
  }
  const removed = new Set();
  for (const node of graph.nodes) {
    if (node.op !== OP.SPLIT) continue;
    const atom = byId.get(node.next), jump = atom && byId.get(atom.next);
    if (!atom || atom.op !== OP.CHAR || !jump || jump.op !== OP.JUMP || jump.target !== node.id) continue;
    if (incoming.get(atom.id) !== 1 || incoming.get(jump.id) !== 1) continue;
    node.op = OP.CHAR_STAR;
    node.args = [...atom.args];
    node.next = node.target;
    node.target = null;
    removed.add(atom.id);
    removed.add(jump.id);
  }
  graph.nodes = graph.nodes.filter(node => !removed.has(node.id));
}

function reachable(graph) {
  const byId = new Map(graph.nodes.map(node => [node.id, node]));
  const visited = new Set(), pending = [graph.entry];
  while (pending.length) {
    const id = pending.pop();
    if (id === null || visited.has(id)) continue;
    const node = byId.get(id);
    assert(node, `missing node ${id}`);
    visited.add(id);
    pending.push(node.next, node.target);
  }
  graph.nodes = graph.nodes.filter(node => visited.has(node.id));
}

function shareTails(graph) {
  const byId = new Map(graph.nodes.map(node => [node.id, node]));
  function skipJump(id) {
    const visited = new Set();
    while (id !== null && byId.get(id).op === OP.JUMP && !visited.has(id)) {
      visited.add(id);
      id = byId.get(id).target;
    }
    return id;
  }
  graph.entry = skipJump(graph.entry);
  for (const node of graph.nodes) {
    node.next = skipJump(node.next);
    node.target = skipJump(node.target);
  }
  reachable(graph);
  let partitions = new Map(graph.nodes.map(node => [node.id, 0]));
  for (let round = 0; round <= graph.nodes.length; round++) {
    const classes = new Map(), refined = new Map();
    for (const node of graph.nodes) {
      const signature = JSON.stringify([node.op, node.args,
        node.next === null ? null : partitions.get(node.next),
        node.target === null ? null : partitions.get(node.target)]);
      if (!classes.has(signature)) classes.set(signature, classes.size);
      refined.set(node.id, classes.get(signature));
    }
    const stable = graph.nodes.every(node => partitions.get(node.id) === refined.get(node.id));
    partitions = refined;
    if (stable) break;
  }
  const representatives = new Map();
  for (const node of graph.nodes) representatives.set(partitions.get(node.id), node.id);
  const remap = id => id === null ? null : representatives.get(partitions.get(id));
  graph.entry = remap(graph.entry);
  graph.nodes = graph.nodes.filter(node => remap(node.id) === node.id);
  for (const node of graph.nodes) {
    node.next = remap(node.next);
    node.target = remap(node.target);
  }
  reachable(graph);
}

function encode(graph, shortBranches = false, escaped = false) {
  const ordered = [...graph.nodes];
  const entryIndex = ordered.findIndex(node => node.id === graph.entry);
  if (entryIndex !== 0) ordered.unshift(...ordered.splice(entryIndex, 1));
  const records = [];
  for (let index = 0; index < ordered.length; index++) {
    const node = ordered[index];
    records.push({...node, short: false});
    if (node.next !== null && node.next !== ordered[index + 1]?.id) {
      records.push({id: `after-${node.id}`, op: OP.JUMP, args: [], target: node.next, short: false});
    }
  }
  const branch = record => record.op === OP.JUMP || record.op === OP.SPLIT;
  const longBytes = escaped ? FORMAT.ESCAPED_LONG_BYTES : FORMAT.LONG_BRANCH_BYTES;
  const width = record => branch(record) ? record.short ? FORMAT.SHORT_BRANCH_BYTES : longBytes : 1 + record.args.length;
  function layout() {
    let position = FORMAT.HEADER;
    const offsets = new Map();
    for (const record of records) {
      offsets.set(record.id, position);
      position += width(record);
    }
    return offsets;
  }
  if (shortBranches) {
    let changed;
    do {
      changed = false;
      const offsets = layout();
      for (const record of records) {
        if (!branch(record) || record.short) continue;
        const start = offsets.get(record.id), target = offsets.get(record.target);
        const relative = target - start - FORMAT.SHORT_BRANCH_BYTES - (target > start ? longBytes - FORMAT.SHORT_BRANCH_BYTES : 0);
        if (relative >= FORMAT.SHORT_MIN + (escaped ? 1 : 0) && relative <= FORMAT.SHORT_MAX) {
          record.short = true;
          changed = true;
        }
      }
    } while (changed);
  }
  const offsets = layout(), bytes = [graph.slots];
  for (const record of records) {
    if (!branch(record)) bytes.push(record.op, ...record.args);
    else {
      const relative = offsets.get(record.target) - offsets.get(record.id) - width(record);
      assert(relative >= FORMAT.LONG_MIN && relative <= FORMAT.LONG_MAX, "jump out of word range");
      if (record.short) {
        assert(relative >= FORMAT.SHORT_MIN && relative <= FORMAT.SHORT_MAX);
        bytes.push(escaped ? record.op : record.op === OP.SPLIT ? OP.SPLIT_SHORT : OP.JUMP_SHORT, relative & FORMAT.BYTE_MASK);
      } else {
        bytes.push(record.op);
        if (escaped) bytes.push(FORMAT.ESCAPE);
        bytes.push(relative & FORMAT.BYTE_MASK, relative >> FORMAT.BYTE_BITS & FORMAT.BYTE_MASK);
      }
    }
  }
  return Uint8Array.from(bytes);
}

function transform(bytes, options = {}) {
  const graph = decode(bytes);
  compactAtoms(graph, options);
  if (options.star) fuseStars(graph);
  const original = encode(graph, options.short, options.escape);
  if (!options.tail) return original;
  const shared = clone(graph);
  shareTails(shared);
  try {
    const candidate = encode(shared, options.short, options.escape);
    return candidate.length < original.length ? candidate : original;
  } catch (error) {
    if (error.message === "jump out of word range") return original;
    throw error;
  }
}

function compile(pattern, options, backend) {
  return transform(compileRe(pattern, options), backend);
}

module.exports = {OP, FORMAT, decode, encode, transform, compile};
