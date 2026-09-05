#!/usr/bin/env node
"use strict";

const fs = require("node:fs");
const path = require("node:path");
const report = require("./results.json");
const folder = path.join(__dirname, "reports");
fs.mkdirSync(folder, {recursive: true});

const descriptions = {
  baseline: "Unmodified instruction format; the readable experimental VM reproduces the original 546-byte M4 object.",
  char: "An ASCII, nonzero, nonnegated singleton class becomes CHAR plus one operand byte. All other classes keep their original representation. Input still uses the original decoder, including its malformed UTF-8 behavior.",
  short: "Separate short SPLIT/JUMP opcodes carry signed 8-bit offsets. Original signed 16-bit forms remain available. Iterative relaxation relocates every target after shrinking; no offset truncation is allowed.",
  char_short: "CHAR and separate short-branch opcodes together.",
  star: "A greedy SPLIT–CHAR–JUMP loop becomes CHAR_STAR. Its VM still pushes one complete capture snapshot per choice, including the final failed iteration. Plus uses CHAR followed by CHAR_STAR. Lazy, grouped and general-class repeats retain the original instructions.",
  tail: "Jump threading, unreachable-node removal and graph partition refinement merge equivalent continuations. Equivalence includes opcode operands, capture slots and ordered successors. Serialization adds explicit jumps when necessary; the result is accepted only if the fully relocated program is smaller. The existing VM executes it unchanged.",
  range: "A nonnegated single ASCII range becomes ASCII_RANGE plus two bounds. General and negated classes remain unchanged.",
  all: "CHAR, separate short branches, greedy CHAR_STAR and graph sharing together.",
  all_range: "All transformations, including ASCII_RANGE.",
  short_shared: "Same bytes as short, with a shared C dispatch path for all four branch forms.",
  char_short_shared: "Same bytes as char_short, with a shared C branch dispatch path.",
  all_shared: "Same bytes as all, with shared branch dispatch.",
  escape: "SPLIT and JUMP each use one signed operand byte in [-127,127]. Operand 0x80 introduces a signed 16-bit displacement. Long instructions therefore cost four bytes; short ones cost two. This format requires its dedicated VM build.",
  char_escape: "CHAR and escaped variable-width branches. It fits the 600-byte M4 VM ceiling.",
  char_tail: "CHAR and graph sharing, retaining the original long-branch encoding.",
  char_escape_tail: "CHAR, escaped variable-width branches and graph sharing. This is the preferred measured option under the 600-byte VM ceiling.",
};

function percent(result) {
  const totals = result.uniqueCorpusPrograms;
  return (100 * (totals.baseline - totals.candidate) / totals.baseline).toFixed(2);
}

function sampleTable(names) {
  const lines = ["| Pattern (search enabled) | " + names.join(" | ") + " |",
    "|---|" + names.map(() => "---:").join("|") + "|"];
  report.samples.forEach((sample, index) => {
    if (sample.search) lines.push(`| ${JSON.stringify(sample.pattern).replaceAll("|", "&#124;")} | ` +
      names.map(name => report.variants[name].sampleSizes[index]).join(" | ") + " |");
  });
  return lines.join("\n");
}

for (const [name, result] of Object.entries(report.variants)) {
  const totals = result.uniqueCorpusPrograms;
  const m4 = result.sizes["cortex-m4"], m0 = result.sizes["cortex-m0plus"];
  const compiler = report.compilerVariants[name];
  const flags = [...["char", "short", "star", "range", "escape"].map(key =>
    `-DENABLE_${key.toUpperCase()}=${result.options[key] ? 1 : 0}`), `-DSHARED_BRANCH=${result.options.shared ? 1 : 0}`];
  const lines = [`# ${name}`, "", descriptions[name], "", "## Measured sizes", "",
    `- Cortex-M4: ${m4.flash} B object flash; ${m4.linkedFlash} B linked flash.`,
    `- Cortex-M0+: ${m0.flash} B object flash; ${m0.linkedFlash} B linked flash (including required libgcc helper).`,
    `- ${totals.programs} distinct original corpus bytecode programs: ${totals.baseline} → ${totals.candidate} B (${percent(result)}% saved).`,
    compiler ? `- On-device C compiler + optional repacker + VM: ${compiler.combined["cortex-m4"].flash} B; linked ${compiler.combined["cortex-m4"].linkedFlash} B. C output parity: ${compiler.compared}/${compiler.compared}.`
      : "- This exact combination has no on-device C graph compiler measurement. Its build-time JavaScript backend is executable; its cost is not claimed as zero in an embedded compiler.",
    "", "## Tests", "",
    `ASan/UBSan: ${report.cases} differential cases, ${result.failures.length} semantic mismatches, ${result.resourceDifferences.length} bounded-budget differences. Every compared successful match checks all capture slots. Resource differences are rerun with sufficient limits and must then agree.`,
    "", "## Concrete programs", "", sampleTable(["baseline", name]), "",
    "Every program's full emitted hex, including non-search samples, is in `../bytecode_samples.json`. These sizes come from assembled byte arrays executed by the C VM, not an opcode-count estimate.",
    "", "## Reproduction", "", "```sh", "node ../run.js", "node ../report.js", "```", "",
    "VM flags (in addition to the protocol flags in results.json):", "", "```text", flags.join(" "), "```", ""];
  fs.writeFileSync(path.join(folder, name + ".md"), lines.join("\n"));
}

const lines = ["# Measured bytecode backend experiments", "",
  "The four requested steps are implemented: CHAR, short/long branches, fused atom repetition and graph-based tail sharing. All work is isolated in this directory, with a frozen snapshot of the original VM, compilers and corpora. The original src/ engine and its format are unchanged.", "",
  "## Result", "",
  "The measured choice under the 600-byte Cortex-M4 VM ceiling is **CHAR + escaped branches + tail sharing: 584 B VM and 27.71% smaller bytecode** on distinct corpus programs. Tail sharing alone saves 8.37% and runs on the existing 546 B VM. These numbers do not solve the 1400 B all-in engine target: on-device compiler costs remain much larger.", "",
  "## Measurement protocol", "",
  `Toolchain: ${report.toolchain}. The exact flags and snapshot SHA-256 hashes are in results.json.`, "",
  "Object flash means .text + .rodata + initialized data, consistent with tools/measure.sh. Linked flash includes alignment and any pulled-in libgcc routines, with no libc or startup. M0+ needs __gnu_thumb1_case_uqi in several builds; omitting it would undercount the image. The linked images have no unresolved symbols. Pattern bytecode is additional to VM flash. JavaScript build-tool size is not target flash.", "",
  "## All executable VM variants", "",
  "| Variant | M4 object B | M4 linked B | M0+ linked B | Corpus bytecode B | Saved |",
  "|---|---:|---:|---:|---:|---:|"];
for (const [name, result] of Object.entries(report.variants)) lines.push(
  `| [${name}](reports/${name}.md) | ${result.sizes["cortex-m4"].flash} | ${result.sizes["cortex-m4"].linkedFlash} | ${result.sizes["cortex-m0plus"].linkedFlash} | ${result.uniqueCorpusPrograms.candidate} | ${percent(result)}% |`);
lines.push("", "The corpus totals use 3587 distinct original bytecode programs (deduplicated by original bytes), not frequency-weighted input cases. The baseline sum is 261247 B. Per-seed weighted totals are also preserved in results.json.", "",
  "## Program sizes", "", sampleTable(["baseline", "char", "short", "star", "tail", "char_escape_tail"]), "",
  "## On-device C compiler cost", "",
  "These are compiled C implementations, not estimates. The compact-atom compiler emits CHAR/RANGE directly. Branch relaxation and star fusion use a separate C repacking pass. This pass is a cost probe, not a size-optimized parser redesign. Graph sharing currently runs only in the JavaScript backend.", "",
  "| C backend | Parser/emitter B | Compiler + repacker + VM B | Linked B | Byte-for-byte parity |",
  "|---|---:|---:|---:|---:|");
for (const [name, result] of Object.entries(report.compilerVariants)) lines.push(
  `| ${name} | ${result.sizes["cortex-m4"].flash} | ${result.combined["cortex-m4"].flash} | ${result.combined["cortex-m4"].linkedFlash} | ${result.compared}/${result.compared} |`);
lines.push("", "C parity covers all 3309 distinct successfully JS-compiled non-i (pattern, flags) pairs in the frozen corpora. C Unicode-i compilation remains unsupported, as in the original compiler; the VM/backend tests retain Unicode-i expansion from JavaScript. The previous conversation's 3628/3628 claim was not reproduced by this dataset and is not used here.", "",
  "Compiler scratch remains 1016 B. The repacker additionally requires a caller-owned array of 24-byte records, one per input instruction (allocating input_length records is a conservative bound), plus separate input/output bytecode buffers. All buffers must be disjoint; re_repack accepts trusted compiler output. The compiler may temporarily emit more bytes than the final program, e.g. before removing a {0} body. C parity tests use a 64 KiB output buffer; the final bytecode length alone is not a sufficient compiler capacity guarantee.", "",
  "## Validation and limits", "",
  `- ${report.cases} cases per VM build, including all 4839 accepted corpus rows, extra generated combinations, long relocations, malformed UTF-8 and resource boundaries.`,
  "- All 16 builds have zero semantic mismatches under ASan/UBSan; all successful results compare every capture slot with the snapshot VM.",
  "- Fused-star builds differ on three deliberately small instruction budgets: fusion executes fewer dispatch steps. All three agree when rerun with sufficient budget. The instruction budget remains enforced, but is not an invariant amount of logical regex work across representations.",
  "- The original compiler rejects 367 corpus rows; their names, patterns and errors are retained in results.json. They are not classified as successful VM tests.",
  "- C repacker tests exercise zero output capacity, insufficient scratch, too-small output and invalid jump targets. The public trust boundary still excludes arbitrary untrusted bytecode.",
  "- Host execution validates the C implementations; M4/M0+ figures are cross-compiled and linked. No board timing or hardware execution claim is made.", "",
  "## Deployment and further directions", "",
  "1. **Tail sharing first:** it requires no VM change and saves 21855 B across the distinct-program corpus. A precise result comes from the actual fully relocated candidate; longer results fall back to the original program.",
  "2. **CHAR + escape + tail for a new format:** +38 B of VM object flash buys 72380 B of aggregate bytecode saving. Corpus-average break-even is two distinct patterns, but application-specific patterns determine the real result. The escaped branch format is incompatible with the old operand encoding, so its compiler output and VM must ship together.",
  "3. **Keep fused CHAR_STAR optional:** its extra dispatch and snapshot code exceeds the 600 B VM ceiling. The 686 B shared-dispatch combination saves 30.44%; that is useful only when pattern storage outweighs the larger VM.",
  "4. **Next bytecode target: counted repetition.** ^a{100}$ still takes 218 B in the selected format versus 416 B originally. A counted-repeat instruction could remove this linear bytecode expansion; it must account for counters and capture restoration before a size claim is justified.",
  "5. **Next compiler target: direct compact emission.** The measured C repacker adds substantial flash and scratch. A compiler emitting the selected representation directly should be compared against the measured 3446 B char_escape toolchain, with Unicode folding cost still explicit. No unmeasured savings are claimed.",
  "6. **Tail layout improvements:** the current pass merges equivalent continuations and then restores fallthrough with jumps. A layout optimized for frequently shared successors might remove more jumps without expanding the VM. The current fully relocated length gate is the acceptance criterion.", "",
  "Assembly hot paths, tagged workspace records and native post-link optimization do not change regex bytecode size. They are not assigned invented bytecode savings. A pattern-specific native-code generator would replace bytecode with machine code; calling its bytecode 0 B is not a useful total-size result, and no working native generator is claimed by these experiments.", "",
  "## Reproduce", "", "Run from this directory:", "", "```sh", "node run.js", "node report.js", "```", "",
  "run.js compiles and links both ARM targets, builds sanitizer-enabled host differential runners, runs all VM and C-compiler comparisons, and writes results.json and bytecode_samples.json. report.js regenerates this report and one report per variant. Build intermediates stay in ignored build/. A complete sample hex dump is provided for every variant and pattern.", "");
fs.writeFileSync(path.join(__dirname, "REPORT.md"), lines.join("\n"));
