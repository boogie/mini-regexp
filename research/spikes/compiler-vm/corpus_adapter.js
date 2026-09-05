#!/usr/bin/env node
"use strict";

const path = require("node:path");
const {spawnSync} = require("node:child_process");
const readline = require("node:readline");
const {compileRe} = require("../../../tools/re_compile.js");

function execute(argv) {
  const args = [...argv];
  const ignoreCase = args[0] === "-i";
  if (ignoreCase) args.shift();
  if (args.length !== 2) return "error\n";

  let program;
  try {
    program = compileRe(args[0], {ignoreCase, tail: process.env.TINYRE_NO_TAIL ? false : undefined});
  } catch {
    return "error\n";
  }

  const helper = process.env.TINYRE_VM_ADAPTER || path.join(__dirname, "tinyre_vm_adapter");
  const result = spawnSync(helper, [Buffer.from(program).toString("hex"), args[1]], {
    encoding: "utf8",
    timeout: 1800,
  });
  if (result.error || result.signal || result.status !== 0) return "error\n";
  return result.stdout;
}

async function batch() {
  const input = readline.createInterface({input: process.stdin, crlfDelay: Infinity});
  for await (const line of input) {
    try {
      const request = JSON.parse(line);
      const args = request.flags.includes("i")
        ? ["-i", request.pat, request.text]
        : [request.pat, request.text];
      process.stdout.write(`${JSON.stringify(execute(args))}\n`);
    } catch {
      process.stdout.write('"error\\n"\n');
    }
  }
}

if (process.argv[2] === "--batch") batch();
else process.stdout.write(execute(process.argv.slice(2)));
