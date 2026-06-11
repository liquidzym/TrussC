#!/usr/bin/env node
import { runJsonJob, runTextToImage } from "../src/index.mjs";

function parseArgs(argv) {
  const args = {};
  for (let i = 0; i < argv.length; ++i) {
    const item = argv[i];
    if (!item.startsWith("--")) {
      continue;
    }
    const key = item.slice(2).replace(/-([a-z])/g, (_, c) => c.toUpperCase());
    const next = argv[i + 1];
    if (!next || next.startsWith("--")) {
      args[key] = true;
    } else {
      args[key] = next;
      i += 1;
    }
  }
  return args;
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  const result = args.job
    ? await runJsonJob(args.job, args)
    : await runTextToImage({
        model: args.model || "ideogram4-q4_0",
        prompt: args.prompt || "",
        output: args.output,
        width: args.width ? Number(args.width) : undefined,
        height: args.height ? Number(args.height) : undefined,
        steps: args.steps ? Number(args.steps) : undefined,
        seed: args.seed ? Number(args.seed) : undefined,
        backend: args.backend || "cuda0",
        paramsBackend: args.paramsBackend || "cpu",
        offloadToCpu: args.offloadToCpu !== "false",
        diffusionFlashAttention: args.diffusionFlashAttention !== "false"
      });
  console.log(JSON.stringify(result, null, 2));
}

main().catch((error) => {
  console.error(error.stack || String(error));
  process.exit(1);
});
