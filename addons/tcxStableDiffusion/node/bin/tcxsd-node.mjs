#!/usr/bin/env node
import {
  TcxSdError,
  cancelImageJob,
  createControlNetRequest,
  createImageToImageRequest,
  createInpaintRequest,
  createLoraStackRequest,
  createRefineRequest,
  createTextToImageRequest,
  createUpscaleRequest,
  runJsonJob,
  runTextToImage
} from "../src/index.mjs";

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

function booleanArg(value) {
  if (value === undefined) return undefined;
  if (value === true) return true;
  return String(value).toLowerCase() !== "false";
}

function parseLoras(value) {
  if (!value || value === true) return [];
  return String(value).split(",").map((item) => {
    const [path, weight] = item.split(":");
    return { path, weight: weight ? Number(weight) : 1 };
  }).filter((item) => item.path);
}

function buildDirectRequest(args, normalizedArgs) {
  const common = {
    model: args.model || "ideogram4-q4_0",
    prompt: args.prompt || "",
    output: args.output,
    sidecar: args.sidecar,
    quality: args.quality,
    width: args.width ? Number(args.width) : undefined,
    height: args.height ? Number(args.height) : undefined,
    steps: args.steps ? Number(args.steps) : undefined,
    seed: args.seed ? Number(args.seed) : undefined,
    cfgScale: args.cfgScale ? Number(args.cfgScale) : undefined,
    strength: args.strength ? Number(args.strength) : undefined,
    controlStrength: args.controlStrength ? Number(args.controlStrength) : undefined,
    upscaleFactor: args.upscaleFactor ? Number(args.upscaleFactor) : undefined,
    initImage: args.initImage,
    maskImage: args.maskImage,
    controlImage: args.controlImage,
    sourceImage: args.sourceImage,
    loras: parseLoras(args.lora || args.loras),
    runtimePreset: args.runtimePreset,
    manageServer: booleanArg(args.reuseServer) ? false : undefined,
    backend: args.backend,
    paramsBackend: args.paramsBackend,
    offloadToCpu: booleanArg(args.offloadToCpu),
    diffusionFlashAttention: booleanArg(args.diffusionFlashAttention),
    streamLayers: booleanArg(args.streamLayers),
    maxVramGiB: normalizedArgs.maxVramGiB,
    host: args.host || "127.0.0.1",
    port: args.port ? Number(args.port) : 1234,
    loraModelDir: args.loraModelDir
  };
  const mode = String(args.mode || "textToImage").toLowerCase();
  if (mode === "img2img" || mode === "imagetoimage" || mode === "image_to_image") return createImageToImageRequest(common);
  if (mode === "inpaint") return createInpaintRequest(common);
  if (mode === "controlnet" || mode === "control_net") return createControlNetRequest(common);
  if (mode === "lorastack" || mode === "lora_stack") return createLoraStackRequest(common);
  if (mode === "refine") return createRefineRequest(common);
  if (mode === "upscale") return createUpscaleRequest(common);
  return createTextToImageRequest(common);
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  const normalizedArgs = {
    ...args,
    maxVramGiB: (args.maxVramGiB || args.maxVramGib) ? Number(args.maxVramGiB || args.maxVramGib) : undefined
  };
  if (args.cancel) {
    const result = await cancelImageJob(
      String(args.cancel).startsWith("/")
        ? { poll_url: String(args.cancel) }
        : { id: String(args.cancel) },
      {
        host: args.host || "127.0.0.1",
        port: args.port ? Number(args.port) : 1234
      }
    );
    console.log(JSON.stringify({ ok: true, cancelled: args.cancel, result }, null, 2));
    return;
  }

  const result = args.job
    ? await runJsonJob(args.job, normalizedArgs)
    : await runTextToImage(buildDirectRequest(args, normalizedArgs));
  console.log(JSON.stringify(result, null, 2));
}

main().catch((error) => {
  if (error instanceof TcxSdError) {
    console.error(JSON.stringify({
      ok: false,
      code: error.code,
      error: error.message,
      remediation_hints: error.remediationHints,
      details: error.details
    }, null, 2));
  } else {
    console.error(error.stack || String(error));
  }
  process.exit(1);
});
