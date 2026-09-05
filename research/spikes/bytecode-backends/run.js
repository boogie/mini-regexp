#!/usr/bin/env node
"use strict";

const fs = require("node:fs");
const path = require("node:path");
const crypto = require("node:crypto");
const assert = require("node:assert/strict");
const {spawnSync} = require("node:child_process");
const {compileRe} = require("./snapshot/tools/re_compile.js");
const {decode, encode, transform} = require("./backend.js");

const root = __dirname;
const build = path.join(root, "build");
fs.mkdirSync(build, {recursive: true});
const variants = {
  baseline: {}, char: {char: true}, short: {short: true}, char_short: {char: true, short: true},
  star: {char: true, star: true}, tail: {tail: true}, range: {range: true},
  all: {char: true, short: true, star: true, tail: true},
  all_range: {char: true, short: true, star: true, tail: true, range: true},
  short_shared: {short: true, shared: true}, char_short_shared: {char: true, short: true, shared: true},
  all_shared: {char: true, short: true, star: true, tail: true, shared: true},
  escape: {short: true, escape: true}, char_escape: {char: true, short: true, escape: true},
  char_tail: {char: true, tail: true}, char_escape_tail: {char: true, short: true, escape: true, tail: true},
};
const common = ["-Os", "-std=gnu99", "-DNDEBUG", "-ffreestanding", "-fno-stack-protector",
  "-ffunction-sections", "-fdata-sections", "-fno-unwind-tables", "-fno-asynchronous-unwind-tables", "-fno-pic"];
const hostFlags = ["-std=c99", "-Wall", "-Wextra", "-Werror", "-O1", "-g", "-fsanitize=address,undefined"];

function run(command, args, extra = {}) {
  const result = spawnSync(command, args, {cwd: root, encoding: "utf8", maxBuffer: 128 * 1024 * 1024, ...extra});
  if (result.error || result.status !== 0) throw new Error(`${command} ${args.join(" ")}: ${result.error || result.stderr || result.stdout}`);
  return result.stdout;
}

function definitions(options) {
  return [...["char", "short", "star", "range", "escape"].map(key => `-DENABLE_${key.toUpperCase()}=${options[key] ? 1 : 0}`),
    `-DSHARED_BRANCH=${options.shared ? 1 : 0}`];
}

function measure(source, tag, flags = []) {
  const output = {};
  for (const cpu of ["cortex-m4", "cortex-m0plus"]) {
    const object = path.join(build, `${tag}-${cpu}.o`);
    run("arm-none-eabi-gcc", [...common, "-mcpu=" + cpu, "-mthumb", "-fstack-usage", ...flags, "-c", source, "-o", object]);
    const sizes = run("arm-none-eabi-size", [object]).trim().split("\n").at(-1).trim().split(/\s+/).slice(0, 3).map(Number);
    const undefinedSymbols = run("arm-none-eabi-nm", ["-u", object]).trim();
    const linked = object.replace(/\.o$/, ".elf");
    run("arm-none-eabi-gcc", ["-mcpu=" + cpu, "-mthumb", "-nostdlib", "-Wl,-Ttext=0x1000",
      "-Wl,-e," + (tag.includes("compiler") || tag.includes("combined") ? "re_compile" : "re_match"), object, "-lgcc", "-o", linked]);
    const linkedSizes = run("arm-none-eabi-size", [linked]).trim().split("\n").at(-1).trim().split(/\s+/).slice(0, 3).map(Number);
    assert.equal(run("arm-none-eabi-nm", ["-u", linked]).trim(), "", tag);
    output[cpu] = {flash: sizes[0] + sizes[1], linkedFlash: linkedSizes[0] + linkedSizes[1],
      text_rodata: sizes[0], data: sizes[1], bss: sizes[2], undefinedSymbols};
  }
  return output;
}

function optionsFor(flags = "") {
  return {ignoreCase: flags.includes("i"), multiline: flags.includes("m"), dotAll: flags.includes("s")};
}

const rejected = [];
const cases = [];
for (const filename of ["corpus_wb_seed1.jsonl", "corpus_wb_seed7.jsonl"]) {
  const rows = fs.readFileSync(path.join(root, "snapshot/tests/corpus", filename), "utf8").trim().split("\n");
  rows.forEach((line, index) => {
    const row = JSON.parse(line);
    try {
      const bytes = compileRe(row.pat, optionsFor(row.flags));
      cases.push({name: `${filename}:${index + 1}`, pattern: row.pat, flags: row.flags,
        bytes, text: Buffer.from(row.text), suite: filename});
    } catch (error) {
      rejected.push({name: `${filename}:${index + 1}`, pattern: row.pat, error: error.message});
    }
  });
}

const witnesses = ["", "a", "abc", "[a-z]+", "a*", "a+", "a*?", "a{1,3}",
  "(a|ab)+", "\\bfoo\\b", "^([a-z]+)=([0-9]+)$", "abc|xbc", "(?:abc|xbc)+",
  "^a{100}$", "é", "[a-z]", "[aaab]", "[^a]", "\\W", "[a-zA-Z_]", "(?:a|b)*c"];
const samples = witnesses.flatMap(pattern => [false, true].map(search => ({pattern, search,
  bytes: compileRe(pattern, {search})})));
for (const sample of samples) {
  for (const text of ["", "aaa", "abc", "xbc", "zzxbc", "ax", "É", "é", "_", "a".repeat(110)]) {
    cases.push({...sample, text: Buffer.from(text), name: `${sample.pattern}/${sample.search}/${text}`, suite: "witness"});
  }
}
for (const length of [25, 26, 27, 28, 29, 30, 31, 32, 62, 63, 64, 65, 126, 127, 128, 129, 500]) {
  for (const pattern of [`(?:${"a".repeat(length)})*b`, `${"a".repeat(length)}|b`]) {
    const bytes = compileRe(pattern);
    for (const text of ["b", "a".repeat(length) + "b", "a".repeat(length) + "c"]) {
      cases.push({name: `relocation/${pattern}/${text}`, pattern, bytes, text: Buffer.from(text), suite: "relocation"});
    }
  }
}
for (const hex of ["80", "c1a1", "e081a1", "f08081a1", "c080", "c3", "f0", "f09080", "c3a9", "ff", "c1a161"]) {
  for (const pattern of ["a", "a*", "[a-z]", "[^a]", ".", "^a*$", "(a|.)+", "\\b"]) {
    cases.push({name: `malformed/${pattern}/${hex}`, pattern, bytes: compileRe(pattern), text: Buffer.from(hex, "hex"), suite: "malformed"});
  }
}
for (let sequence = 0; sequence < 160; sequence++) {
  const atom = ["a", "b", "[a-c]", "(a|ab)", "(?:a|b)", "(a)(b)?", "[^b]", "é"][sequence % 8];
  const quantifier = ["*", "+", "?", "{1,3}", "*?", "+?", "{0,2}?", ""][Math.floor(sequence / 8) % 8];
  const pattern = `(?:${atom}${quantifier}|abc)${sequence % 2 ? "$" : "b?"}`;
  for (const text of ["", "a", "b", "ab", "abc", "aaa", "aaab", "baba", "éé", "xab"]) {
    cases.push({name: `generated/${sequence}/${text}`, pattern, bytes: compileRe(pattern), text: Buffer.from(text), suite: "generated"});
  }
}

for (const depth of [0, 1, 2, 4]) {
  for (const steps of [0, 1, 2, 3, 20, 1000000]) {
    for (const pattern of ["a*", "a|ab", "(ab|a)+", "a*?b", "[^a]", "abc|xbc"]) {
      cases.push({name: `resource/${pattern}/${depth}/${steps}`, pattern, bytes: compileRe(pattern),
        text: Buffer.from("aaaaab"), suite: "resource", depth, steps});
    }
  }
}
for (const steps of [25, 30, 40, 50]) {
  const pattern = "^a*b$";
  cases.push({name: `fused-budget/${steps}`, pattern, bytes: compileRe(pattern, {search: false}),
    text: Buffer.from("a".repeat(20) + "b"), suite: "resource", steps});
}

const report = {toolchain: run("arm-none-eabi-gcc", ["--version"]).split("\n")[0],
  flags: common, hostFlags, samples: samples.map(({bytes, ...sample}) => sample),
  cases: cases.length, rejected, variants: {}};
report.baselineVM = measure("snapshot/src/re.c", "original");
report.baselineCompiler = measure("snapshot/research/spikes/c-compiler/re_compile.c", "compiler");
report.baselineCombined = measure("snapshot/research/spikes/c-compiler/combined.c", "combined");
run("cc", [...hostFlags, "-Dre_match=baseline_match", "-c", "snapshot/src/re.c", "-o", path.join(build, "baseline.o")]);
for (const item of [...cases, ...samples]) assert.deepEqual(encode(decode(item.bytes)), item.bytes, "round trip");

for (const [name, options] of Object.entries(variants)) {
  const flags = definitions(options);
  const sizes = measure("vm.c", name, flags);
  const executable = path.join(build, `${name}-diff`);
  run("cc", [...hostFlags, ...flags, "differential.c", "vm.c", path.join(build, "baseline.o"), "-o", executable]);
  const bytecodes = cases.map(item => transform(item.bytes, options));
  const requests = cases.map((item, index) => [Buffer.from(item.bytes).toString("hex"),
    Buffer.from(bytecodes[index]).toString("hex"), item.text.toString("hex") || "-", item.steps ?? 1000000, item.depth ?? 4096].join(" "));
  const output = run(executable, [], {input: requests.join("\n") + "\n"}).trim().split("\n");
  assert.equal(output.length, cases.length);
  let equal = 0;
  const failures = [], resourceDifferences = [], bySuite = {};
  output.forEach((line, index) => {
    const [before, after, same] = line.split(" ").map(Number);
    const item = cases[index];
    if (item.steps === 0) assert.equal(after, -1, item.name);
    if (!bySuite[item.suite]) bySuite[item.suite] = {cases: 0, baselineBytes: 0, candidateBytes: 0};
    bySuite[item.suite].cases++;
    bySuite[item.suite].baselineBytes += item.bytes.length;
    bySuite[item.suite].candidateBytes += bytecodes[index].length;
    if (same) equal++;
    else (before < 0 || after < 0 ? resourceDifferences : failures).push({name: item.name, before, after, line});
  });
  const sampleSizes = samples.map(sample => transform(sample.bytes, options).length);
  if (resourceDifferences.length) {
    const retries = resourceDifferences.map(difference => {
      const index = cases.findIndex(item => item.name === difference.name);
      return [Buffer.from(cases[index].bytes).toString("hex"), Buffer.from(bytecodes[index]).toString("hex"),
        cases[index].text.toString("hex") || "-", 1000000, 4096].join(" ");
    });
    const checked = run(executable, [], {input: retries.join("\n") + "\n"}).trim().split("\n");
    assert(checked.every(line => line.split(" ")[2] === "1"), "resource retry mismatch");
  }
  const sampleHex = samples.map(sample => Buffer.from(transform(sample.bytes, options)).toString("hex"));
  const unique = new Map();
  cases.filter(item => item.suite.endsWith(".jsonl")).forEach(item => unique.set(Buffer.from(item.bytes).toString("hex"), item));
  const totals = [...unique.values()].reduce((total, item) => {
    total.baseline += item.bytes.length;
    total.candidate += transform(item.bytes, options).length;
    return total;
  }, {programs: unique.size, baseline: 0, candidate: 0});
  report.variants[name] = {options, sizes, equal, failures, resourceDifferences, bySuite, sampleSizes, sampleHex, uniqueCorpusPrograms: totals};
  fs.writeFileSync(path.join(root, "results.json"), JSON.stringify(report, null, 2) + "\n");
  console.log(`${name}: M4=${sizes["cortex-m4"].flash} M0+=${sizes["cortex-m0plus"].flash} equal=${equal}/${cases.length} failures=${failures.length} resourceDifferences=${resourceDifferences.length}`);
  assert.equal(failures.length, 0, JSON.stringify(failures.slice(0, 5)));
}

report.compilerVariants = {};
for (const [name, options] of Object.entries({baseline: {}, char: {char: true}, range: {range: true}, char_range: {char: true, range: true},
  short: {short: true}, char_short: {char: true, short: true}, star: {char: true, star: true},
  char_short_star: {char: true, short: true, star: true},
  escape: {short: true, escape: true}, char_escape: {char: true, short: true, escape: true}})) {
  const flags = definitions(options);
  const sizes = measure("compiler.c", `compiler-${name}`, flags);
  const repack = options.short || options.star;
  const combined = measure(repack ? "combined_repack.c" : "combined.c", `combined-${name}`, flags);
  const executable = path.join(build, `compiler-${name}-diff`);
  run("cc", [...hostFlags, ...flags, `-DUSE_REPACK=${repack ? 1 : 0}`, "compiler_diff.c", "compiler.c",
    ...(repack ? ["repack.c"] : []), "-o", executable]);
  if (repack) {
    const boundaryTest = path.join(build, `repack-${name}-boundaries`);
    run("cc", [...hostFlags, ...flags, "repack_test.c", "repack.c", "-o", boundaryTest]);
    run(boundaryTest, []);
  }
  const unique = new Map();
  for (const item of cases) {
    if (!item.suite.endsWith(".jsonl") || (item.flags || "").includes("i")) continue;
    unique.set(JSON.stringify([item.pattern, item.flags]), item);
  }
  const requests = [...unique.values()].map(item => [Buffer.from(item.pattern).toString("hex") || "-",
    Buffer.from(transform(item.bytes, options)).toString("hex"),
    1 + ((item.flags || "").includes("m") ? 2 : 0) + ((item.flags || "").includes("s") ? 4 : 0)].join(" "));
  const output = run(executable, [], {input: requests.join("\n") + "\n"}).trim().split("\n");
  assert.equal(output.length, requests.length);
  const failures = output.flatMap((line, index) => line.split(" ")[1] === "1" ? [] : [{line, name: [...unique.values()][index].name}]);
  report.compilerVariants[name] = {sizes, combined, compared: unique.size, failures};
  console.log(`C compiler ${name}: M4=${sizes["cortex-m4"].flash} combined=${combined["cortex-m4"].flash} parity=${unique.size-failures.length}/${unique.size}`);
  fs.writeFileSync(path.join(root, "results.json"), JSON.stringify(report, null, 2) + "\n");
  assert.equal(failures.length, 0, JSON.stringify(failures.slice(0, 5)));
}

const snapshots = ["src/re.c", "src/re.h", "src/re_bytecode.h", "tools/re_compile.js",
  "research/spikes/c-compiler/re_compile.c", "tests/corpus/corpus_wb_seed1.jsonl", "tests/corpus/corpus_wb_seed7.jsonl"];
report.snapshotSHA256 = Object.fromEntries(snapshots.map(filename => [filename,
  crypto.createHash("sha256").update(fs.readFileSync(path.join(root, "snapshot", filename))).digest("hex")]));
fs.writeFileSync(path.join(root, "results.json"), JSON.stringify(report, null, 2) + "\n");
const bytecodeSamples = Object.fromEntries(Object.entries(report.variants).map(([name, result]) => [name,
  samples.map((sample, index) => ({pattern: sample.pattern, search: sample.search,
    bytes: result.sampleSizes[index], hex: result.sampleHex[index]}))]));
fs.writeFileSync(path.join(root, "bytecode_samples.json"), JSON.stringify(bytecodeSamples, null, 2) + "\n");
