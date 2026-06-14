import { Buffer } from "node:buffer";
import { spawn } from "node:child_process";
import { existsSync, readFileSync } from "node:fs";
import { mkdir, readFile, readdir, rm, stat, writeFile } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const moduleDir = path.dirname(fileURLToPath(import.meta.url));

export const addonRoot = path.resolve(moduleDir, "..", "..");
export const defaultExampleRoot = path.join(addonRoot, "examples", "ideogram4-basic");
export const defaultNativeDir = path.join(addonRoot, "libs", "stable-diffusion", "current");
export const defaultModelRoot = path.join(defaultExampleRoot, "bin", "data", "models");
export const defaultLoraModelDir = path.join(defaultModelRoot, "loras");

export const errorCodes = {
  UNKNOWN: "UNKNOWN",
  CUDA_OOM: "CUDA_OOM",
  MODEL_ASSET_MISSING: "MODEL_ASSET_MISSING",
  SERVER_START_FAILED: "SERVER_START_FAILED",
  BACKEND_UNSUPPORTED: "BACKEND_UNSUPPORTED",
  CANCEL_NOT_INTERRUPTIBLE: "CANCEL_NOT_INTERRUPTIBLE",
  OUTPUT_MISSING: "OUTPUT_MISSING",
  TIMEOUT: "TIMEOUT"
};

const remediationHints = {
  [errorCodes.CUDA_OOM]: [
    "Use runtimePreset: 'lowVram' or reduce width, height, steps, or batchCount.",
    "Set paramsBackend: 'cpu', offloadToCpu: true, streamLayers: true, and a maxVramGiB budget.",
    "Close other GPU-heavy apps before retrying."
  ],
  [errorCodes.MODEL_ASSET_MISSING]: [
    "Run python tools/setup_sd.py download-model --model <model-id>.",
    "Check modelDir/modelRoot points at the folder containing every required asset."
  ],
  [errorCodes.SERVER_START_FAILED]: [
    "Confirm sd-server exists under libs/stable-diffusion/current/bin.",
    "Check the server log path in the sidecar metadata.",
    "Try another port or stop the process currently using the configured port."
  ],
  [errorCodes.BACKEND_UNSUPPORTED]: [
    "Use the persistent sd-server backend for this request.",
    "Remove the unsupported field or route the job through a backend that supports it."
  ],
  [errorCodes.CANCEL_NOT_INTERRUPTIBLE]: [
    "The cancel request was sent, but upstream may finish the active step first.",
    "Use a shorter timeoutMs or restart the managed server for immediate recovery."
  ],
  [errorCodes.OUTPUT_MISSING]: [
    "Open the backend log path recorded in the sidecar metadata.",
    "Check outputRoot/output permissions and free disk space."
  ],
  [errorCodes.TIMEOUT]: [
    "Increase timeoutMs for final-quality jobs.",
    "Use draft quality first to confirm the model and prompt path work."
  ],
  [errorCodes.UNKNOWN]: [
    "Check the backend log path recorded in the sidecar metadata.",
    "Inspect resolved args/body before retrying."
  ]
};

export function classifyError(message = "") {
  const text = String(message).toLowerCase();
  if (!text) return errorCodes.UNKNOWN;
  if (text.includes("out of memory") || text.includes("cuda oom") || text.includes("cuda_error_out_of_memory")) {
    return errorCodes.CUDA_OOM;
  }
  if (text.includes("missing model asset") || text.includes("does not exist") || text.includes("was not found")) {
    return errorCodes.MODEL_ASSET_MISSING;
  }
  if (text.includes("did not become ready") || text.includes("failed to start sd-server") || text.includes("not reachable")) {
    return errorCodes.SERVER_START_FAILED;
  }
  if (text.includes("backend_unsupported") || text.includes("unsupported") || text.includes("not supported")) {
    return errorCodes.BACKEND_UNSUPPORTED;
  }
  if (text.includes("may not interrupt") || (text.includes("cancel") && text.includes("interrupt"))) {
    return errorCodes.CANCEL_NOT_INTERRUPTIBLE;
  }
  if (text.includes("no image payload") || text.includes("without b64_json") || text.includes("returned no images")) {
    return errorCodes.OUTPUT_MISSING;
  }
  if (text.includes("timed out") || text.includes("timeout")) {
    return errorCodes.TIMEOUT;
  }
  return errorCodes.UNKNOWN;
}

export function errorPayload(message = "", code = undefined) {
  const resolved = code || classifyError(message);
  return {
    code: resolved,
    message: String(message || ""),
    remediation_hints: [...(remediationHints[resolved] || remediationHints[errorCodes.UNKNOWN])]
  };
}

export class TcxSdError extends Error {
  constructor(message, options = {}) {
    super(message, options.cause ? { cause: options.cause } : undefined);
    const payload = errorPayload(message, options.code);
    this.name = "TcxSdError";
    this.code = payload.code;
    this.remediationHints = payload.remediation_hints;
    this.details = options.details;
  }
}

export const modelProfiles = {
  "ideogram4-q4_0": {
    family: "Ideogram4",
    assets: {
      diffusion: "ideogram4-Q4_0.gguf",
      uncond_diffusion: "ideogram4_uncond-Q4_0.gguf",
      llm: "Qwen3VL-8B-Instruct-Q4_K_M.gguf",
      vae: "flux2_ae.safetensors"
    },
    quality: {
      draft: { width: 512, height: 512, steps: 8, cfgScale: 7.0, sampler: "euler" },
      balanced: { width: 1024, height: 1024, steps: 20, cfgScale: 7.0, sampler: "euler" },
      final: { width: 1024, height: 1024, steps: 28, cfgScale: 7.0, sampler: "euler" }
    },
    runtime: {
      default: { backend: "cuda0,te=cpu", paramsBackend: "cpu", offloadToCpu: true, diffusionFlashAttention: true, mmap: true, streamLayers: true, maxVramGiB: 8 },
      lowVram: { backend: "cuda0,te=cpu", paramsBackend: "cpu", offloadToCpu: true, diffusionFlashAttention: true, mmap: true, streamLayers: true, maxVramGiB: 8 },
      rtx4090FullSpeed: { backend: "cuda0", paramsBackend: "cuda0", offloadToCpu: false, diffusionFlashAttention: true, mmap: true, streamLayers: false, maxVramGiB: 0 }
    }
  },
  "flux2-klein-4b-q4_0": {
    family: "FLUX.2-klein",
    assets: {
      diffusion: "flux-2-klein-4b-Q4_0.gguf",
      llm: "Qwen3-4B-Q4_K_M.gguf",
      vae: "flux2_ae.safetensors"
    },
    quality: {
      draft: { width: 512, height: 512, steps: 4, cfgScale: 1.0, sampler: "euler" },
      balanced: { width: 768, height: 768, steps: 6, cfgScale: 1.0, sampler: "euler" },
      final: { width: 1024, height: 1024, steps: 8, cfgScale: 1.0, sampler: "euler" }
    },
    runtime: {
      default: { backend: "cuda0", paramsBackend: "cpu", offloadToCpu: true, diffusionFlashAttention: true, mmap: true, streamLayers: false, maxVramGiB: 0 },
      lowVram: { backend: "cuda0,te=cpu", paramsBackend: "cpu", offloadToCpu: true, diffusionFlashAttention: true, mmap: true, streamLayers: true, maxVramGiB: 6 },
      rtx4090FullSpeed: { backend: "cuda0", paramsBackend: "cuda0", offloadToCpu: false, diffusionFlashAttention: true, mmap: true, streamLayers: false, maxVramGiB: 0 }
    }
  },
  "z-image-turbo-q3_k": {
    family: "Z-Image",
    assets: {
      diffusion: "z_image_turbo-Q3_K.gguf",
      llm: "Qwen3-4B-Instruct-2507-Q4_K_M.gguf",
      vae: "z_image_ae.safetensors"
    },
    quality: {
      draft: { width: 768, height: 512, steps: 4, cfgScale: 1.0, sampler: "euler" },
      balanced: { width: 1024, height: 512, steps: 8, cfgScale: 1.0, sampler: "euler" },
      final: { width: 1280, height: 768, steps: 12, cfgScale: 1.0, sampler: "euler" }
    },
    runtime: {
      default: { backend: "cuda0,te=cpu", paramsBackend: "cpu", offloadToCpu: true, diffusionFlashAttention: true, mmap: true, streamLayers: true, maxVramGiB: 8 },
      lowVram: { backend: "cuda0,te=cpu", paramsBackend: "cpu", offloadToCpu: true, diffusionFlashAttention: true, mmap: true, streamLayers: true, maxVramGiB: 6 },
      rtx4090FullSpeed: { backend: "cuda0", paramsBackend: "cuda0", offloadToCpu: false, diffusionFlashAttention: true, mmap: true, streamLayers: false, maxVramGiB: 0 }
    }
  },
  "sd15-controlnet-canny": {
    family: "SD 1.5 ControlNet Canny",
    assets: {
      model: "v1-5-pruned-emaonly.safetensors",
      control_net: "control_v11p_sd15_canny_fp16.safetensors"
    },
    quality: {
      draft: { width: 512, height: 512, steps: 12, cfgScale: 7.5, sampler: "euler" },
      balanced: { width: 512, height: 512, steps: 20, cfgScale: 7.5, sampler: "euler" },
      final: { width: 768, height: 768, steps: 28, cfgScale: 7.5, sampler: "euler" }
    },
    runtime: {
      default: { backend: "cuda0", paramsBackend: "cpu", offloadToCpu: true, diffusionFlashAttention: true, mmap: true, streamLayers: false, maxVramGiB: 0 },
      lowVram: { backend: "cuda0", paramsBackend: "cpu", offloadToCpu: true, diffusionFlashAttention: true, mmap: true, streamLayers: true, maxVramGiB: 6 },
      rtx4090FullSpeed: { backend: "cuda0", paramsBackend: "cuda0", offloadToCpu: false, diffusionFlashAttention: true, mmap: true, streamLayers: false, maxVramGiB: 0 }
    }
  }
};

const roleArgs = {
  model: "-m",
  diffusion: "--diffusion-model",
  uncond_diffusion: "--uncond-diffusion-model",
  llm: "--llm",
  vae: "--vae",
  control_net: "--control-net"
};

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
const loraExtensions = new Set([".safetensors", ".sft", ".pt", ".ckpt", ".bin"]);

function profileFor(modelId) {
  const profile = modelProfiles[modelId];
  if (!profile) {
    throw new TcxSdError(`Unknown model '${modelId}'. Known models: ${Object.keys(modelProfiles).join(", ")}`, {
      code: errorCodes.MODEL_ASSET_MISSING
    });
  }
  return profile;
}

function normalizeRuntimePreset(value = "default") {
  const text = String(value || "default").replace(/[-_ ]+([a-z0-9])/gi, (_, c) => c.toUpperCase());
  const aliases = {
    lowvram: "lowVram",
    lowVramCuda: "lowVram",
    "4090": "rtx4090FullSpeed",
    rtx4090: "rtx4090FullSpeed",
    fullSpeed: "rtx4090FullSpeed"
  };
  return aliases[text] || text || "default";
}

function qualityDefaults(modelId, quality = "balanced") {
  const profile = profileFor(modelId);
  const key = String(quality || "balanced");
  const defaults = profile.quality[key] || profile.quality.balanced;
  return { ...defaults };
}

function runtimeDefaults(modelId, options = {}) {
  const profile = profileFor(modelId);
  const key = normalizeRuntimePreset(options.runtimePreset || options.runtime?.preset || "default");
  const defaults = profile.runtime[key] || profile.runtime.default;
  return {
    ...defaults,
    ...Object.fromEntries(Object.entries({
      backend: options.backend,
      paramsBackend: options.paramsBackend,
      offloadToCpu: options.offloadToCpu,
      diffusionFlashAttention: options.diffusionFlashAttention,
      mmap: options.mmap,
      streamLayers: options.streamLayers,
      maxVramGiB: options.maxVramGiB
    }).filter(([, value]) => value !== undefined))
  };
}

export function resolveModelDir(modelId, options = {}) {
  if (options.modelDir) {
    return path.resolve(options.modelDir);
  }
  profileFor(modelId);
  return path.join(options.modelRoot || defaultModelRoot, modelId);
}

export function resolveServerExecutable(options = {}) {
  if (options.serverExecutable) {
    return path.resolve(options.serverExecutable);
  }
  const exe = process.platform === "win32" ? "sd-server.exe" : "sd-server";
  return path.join(options.nativeDir || defaultNativeDir, "bin", exe);
}

export function resolveStorageRoots(options = {}) {
  const cwd = path.resolve(options.cwd || process.cwd());
  const outputRoot = path.resolve(cwd, options.outputRoot || path.join(defaultExampleRoot, "outputs", "node"));
  const tempRoot = path.resolve(cwd, options.tempRoot || path.join(outputRoot, "tmp"));
  const cacheRoot = path.resolve(cwd, options.cacheRoot || path.join(outputRoot, "cache"));
  return { outputRoot, tempRoot, cacheRoot };
}

function toServerRelativePath(value) {
  return String(value).replace(/\\/g, "/");
}

function resolveLoraModelDir(options = {}) {
  return path.resolve(options.loraModelDir || defaultLoraModelDir);
}

export function normalizeLoraPath(value, options = {}) {
  const text = String(value || "").trim();
  if (!text) {
    throw new TcxSdError("LoRA path is empty", { code: errorCodes.MODEL_ASSET_MISSING });
  }
  const root = resolveLoraModelDir(options);
  const absolute = path.isAbsolute(text) ? path.resolve(text) : path.resolve(root, text);
  const relative = path.relative(root, absolute);
  if (!relative || relative.startsWith("..") || path.isAbsolute(relative)) {
    throw new TcxSdError(`LoRA path '${text}' is outside the LoRA model directory '${root}'.`, {
      code: errorCodes.MODEL_ASSET_MISSING,
      details: { loraModelDir: root, path: text }
    });
  }
  return toServerRelativePath(relative);
}

export async function listLoras(options = {}) {
  const root = resolveLoraModelDir(options);
  const recursive = options.recursive !== false;
  const found = [];

  async function visit(dir) {
    if (!existsSync(dir)) return;
    for (const entry of await readdir(dir, { withFileTypes: true })) {
      const full = path.join(dir, entry.name);
      if (entry.isDirectory()) {
        if (recursive) await visit(full);
        continue;
      }
      if (!entry.isFile()) continue;
      const extension = path.extname(entry.name).toLowerCase();
      if (!loraExtensions.has(extension)) continue;
      const info = await stat(full);
      const relativePath = toServerRelativePath(path.relative(root, full));
      found.push({
        name: path.basename(entry.name, extension),
        filename: entry.name,
        path: full,
        relativePath,
        sizeBytes: info.size,
        modifiedMs: info.mtimeMs
      });
    }
  }

  await visit(root);
  return found.sort((left, right) => left.relativePath.localeCompare(right.relativePath));
}

export async function cleanupStorage(options = {}) {
  const roots = options.roots || resolveStorageRoots(options);
  const olderThanMs = Number(
    options.olderThanMs ?? (options.olderThanSeconds !== undefined ? Number(options.olderThanSeconds) * 1000 : 24 * 60 * 60 * 1000)
  );
  const cutoff = olderThanMs <= 0 ? Number.POSITIVE_INFINITY : Date.now() - olderThanMs;
  const removed = [];
  const groups = [
    [roots.outputRoot, new Set([".json", ".log"])],
    [roots.tempRoot, new Set([".json", ".log", ".tmp", ".part", ".png"])],
    [roots.cacheRoot, new Set([".tmp", ".part"])]
  ];
  for (const [root, suffixes] of groups) {
    await collectCleanup(root, suffixes, cutoff, removed, options.dryRun !== false);
  }
  return removed;
}

async function collectCleanup(root, suffixes, cutoff, removed, dryRun) {
  if (!root || !existsSync(root)) return;
  for (const entry of await readdir(root, { withFileTypes: true })) {
    const full = path.join(root, entry.name);
    if (entry.isDirectory()) {
      await collectCleanup(full, suffixes, cutoff, removed, dryRun);
      continue;
    }
    if (!entry.isFile() || !suffixes.has(path.extname(entry.name))) continue;
    const info = await stat(full);
    if (info.mtimeMs > cutoff) continue;
    removed.push(full);
    if (!dryRun) {
      await rm(full, { force: true });
    }
  }
}

export function buildServerArgs(options = {}) {
  const modelId = options.model || "ideogram4-q4_0";
  const profile = profileFor(modelId);
  const runtime = runtimeDefaults(modelId, options);
  const modelDir = resolveModelDir(modelId, options);
  const args = [
    resolveServerExecutable(options),
    "--listen-ip",
    options.host || "127.0.0.1",
    "--listen-port",
    String(options.port || 1234)
  ];

  for (const [role, filename] of Object.entries(profile.assets)) {
    const flag = roleArgs[role];
    if (flag) {
      args.push(flag, path.join(modelDir, filename));
    }
  }

  if (runtime.backend) args.push("--backend", runtime.backend);
  if (runtime.paramsBackend) args.push("--params-backend", runtime.paramsBackend);
  if (runtime.offloadToCpu) args.push("--offload-to-cpu");
  if (runtime.diffusionFlashAttention !== false) args.push("--diffusion-fa");
  if (runtime.mmap !== false) args.push("--mmap");
  if (runtime.streamLayers) args.push("--stream-layers");
  if (Number(runtime.maxVramGiB || 0) !== 0) args.push("--max-vram", String(runtime.maxVramGiB));
  if (options.loraModelDir) args.push("--lora-model-dir", path.resolve(options.loraModelDir));
  args.push("-v");
  return args;
}

function fileToDataUrl(value) {
  if (!value) return undefined;
  const text = String(value);
  if (text.startsWith("data:")) return text;
  const absolute = path.resolve(text);
  const data = readFileSync(absolute);
  const ext = path.extname(absolute).toLowerCase();
  const mime = ext === ".jpg" || ext === ".jpeg" ? "image/jpeg" : ext === ".webp" ? "image/webp" : "image/png";
  return `data:${mime};base64,${data.toString("base64")}`;
}

function readPngSize(filePath) {
  try {
    const header = readFileSync(filePath);
    if (header.length < 24) return undefined;
    if (header.subarray(0, 8).compare(Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a])) !== 0) return undefined;
    if (header.subarray(12, 16).toString("ascii") !== "IHDR") return undefined;
    return {
      width: header.readUInt32BE(16),
      height: header.readUInt32BE(20)
    };
  } catch {
    return undefined;
  }
}

export const requestModes = {
  TEXT_TO_IMAGE: "text_to_image",
  IMAGE_TO_IMAGE: "image_to_image",
  INPAINT: "inpaint",
  CONTROL_NET: "control_net",
  LORA_STACK: "lora_stack",
  REFINE: "refine",
  UPSCALE: "upscale"
};

function withRequestMetadata(options, requestMode, metadata = {}) {
  return {
    ...options,
    requestMode,
    metadata: {
      ...(options.metadata || {}),
      ...metadata,
      request_mode: requestMode
    }
  };
}

export function createTextToImageRequest(options = {}) {
  return withRequestMetadata(options, requestModes.TEXT_TO_IMAGE);
}

export function createImageToImageRequest(options = {}) {
  if (!options.initImage) throw new TcxSdError("image_to_image requires initImage", { code: errorCodes.MODEL_ASSET_MISSING });
  return withRequestMetadata({ ...options, strength: options.strength ?? 0.75 }, requestModes.IMAGE_TO_IMAGE, {
    init_image: String(options.initImage),
    strength: String(options.strength ?? 0.75)
  });
}

export function createInpaintRequest(options = {}) {
  if (!options.initImage || !options.maskImage) {
    throw new TcxSdError("inpaint requires initImage and maskImage", { code: errorCodes.MODEL_ASSET_MISSING });
  }
  return withRequestMetadata({ ...options, strength: options.strength ?? 0.75 }, requestModes.INPAINT, {
    init_image: String(options.initImage),
    mask_image: String(options.maskImage),
    strength: String(options.strength ?? 0.75)
  });
}

export function createControlNetRequest(options = {}) {
  if (!options.controlImage) throw new TcxSdError("control_net requires controlImage", { code: errorCodes.MODEL_ASSET_MISSING });
  return withRequestMetadata({ ...options, controlStrength: options.controlStrength ?? 1.0 }, requestModes.CONTROL_NET, {
    control_image: String(options.controlImage),
    control_strength: String(options.controlStrength ?? 1.0)
  });
}

export function createLoraStackRequest(options = {}) {
  const loras = Array.isArray(options.loras) ? options.loras : [];
  if (!loras.length) throw new TcxSdError("lora_stack requires at least one LoRA", { code: errorCodes.MODEL_ASSET_MISSING });
  const normalized = loras.map((lora) => ({
    ...lora,
    path: (options.loraModelDir || path.isAbsolute(String(lora.path || "")))
      ? normalizeLoraPath(lora.path, options)
      : toServerRelativePath(lora.path)
  }));
  return withRequestMetadata({ ...options, loras: normalized }, requestModes.LORA_STACK, {
    lora_count: String(loras.length),
    lora_stack: "true"
  });
}

export function createRefineRequest(options = {}) {
  const source = options.sourceImage || options.initImage;
  if (!source) throw new TcxSdError("refine requires sourceImage", { code: errorCodes.MODEL_ASSET_MISSING });
  return withRequestMetadata({ ...options, initImage: source, strength: options.strength ?? 0.35 }, requestModes.REFINE, {
    refine_source_image: String(source),
    init_image: String(source),
    strength: String(options.strength ?? 0.35)
  });
}

export function createUpscaleRequest(options = {}) {
  const source = options.sourceImage || options.initImage;
  if (!source) throw new TcxSdError("upscale requires sourceImage", { code: errorCodes.MODEL_ASSET_MISSING });
  const scale = Number(options.upscaleFactor ?? 2);
  return withRequestMetadata({ ...options, initImage: source, strength: options.strength ?? 0.25, upscaleFactor: scale }, requestModes.UPSCALE, {
    upscale_source_image: String(source),
    init_image: String(source),
    strength: String(options.strength ?? 0.25),
    upscale_factor: String(scale),
    upscale_method: "img2img_refine"
  });
}

export function getBackendCapabilities(options = {}) {
  const mode = options.executionMode || options.mode || "persistent_server";
  const modelId = options.model || "ideogram4-q4_0";
  const profile = modelProfiles[modelId];
  const hasControlNet = Boolean(options.hasControlNet ?? profile?.assets?.control_net);
  const hasLoraDir = Boolean(options.loraModelDir);
  if (mode === "in_process") {
    return { textToImage: true, imageToImage: false, inpaint: false, controlNet: false, loraStack: false, refine: false, upscale: false };
  }
  if (mode === "cli_process") {
    return { textToImage: true, imageToImage: true, inpaint: true, controlNet: hasControlNet, loraStack: false, refine: true, upscale: true };
  }
  return { textToImage: true, imageToImage: true, inpaint: true, controlNet: hasControlNet, loraStack: hasLoraDir, refine: true, upscale: true };
}

export function assertRequestSupported(options = {}, capabilityOptions = {}) {
  const caps = getBackendCapabilities({ model: options.model, ...capabilityOptions });
  const mode = options.requestMode || requestModes.TEXT_TO_IMAGE;
  const checks = {
    [requestModes.TEXT_TO_IMAGE]: caps.textToImage,
    [requestModes.IMAGE_TO_IMAGE]: caps.imageToImage,
    [requestModes.INPAINT]: caps.inpaint,
    [requestModes.CONTROL_NET]: caps.controlNet,
    [requestModes.LORA_STACK]: caps.loraStack,
    [requestModes.REFINE]: caps.refine,
    [requestModes.UPSCALE]: caps.upscale
  };
  if (checks[mode] === false || (options.controlImage && !caps.controlNet) || (options.maskImage && !caps.inpaint) || (options.initImage && !caps.imageToImage) || (options.loras?.length && !caps.loraStack)) {
    throw new TcxSdError(`BACKEND_UNSUPPORTED: ${mode} is not supported by ${capabilityOptions.executionMode || "persistent_server"} for this runtime/model.`, {
      code: errorCodes.BACKEND_UNSUPPORTED,
      details: { requestMode: mode, capabilities: caps }
    });
  }
  return true;
}

export function buildImageRequest(options = {}) {
  const modelId = options.model || "ideogram4-q4_0";
  profileFor(modelId);
  const defaults = qualityDefaults(modelId, options.quality);
  const loras = Array.isArray(options.loras) ? options.loras : [];
  const body = {
    prompt: options.prompt || "",
    negative_prompt: options.negativePrompt || "",
    width: Number(options.width || defaults.width),
    height: Number(options.height || defaults.height),
    strength: Number(options.strength || 0.75),
    seed: Number(options.seed ?? -1),
    batch_count: Number(options.batchCount || 1),
    control_strength: Number(options.controlStrength || 1.0),
    sample_params: {
      sample_method: options.sampler || defaults.sampler || "euler",
      sample_steps: Number(options.steps || defaults.steps),
      guidance: {
        txt_cfg: Number(options.cfgScale || defaults.cfgScale)
      }
    },
    lora: loras.map((lora) => ({
      path: (options.loraModelDir || path.isAbsolute(String(lora.path || "")))
        ? normalizeLoraPath(lora.path, options)
        : toServerRelativePath(lora.path),
      multiplier: Number(lora.weight ?? lora.multiplier ?? 1.0),
      is_high_noise: Boolean(lora.isHighNoise || lora.is_high_noise)
    })),
    output_format: "png",
    output_compression: 100
  };
  for (const [optionKey, bodyKey] of [
    ["initImage", "init_image"],
    ["maskImage", "mask_image"],
    ["controlImage", "control_image"]
  ]) {
    const encoded = fileToDataUrl(options[optionKey]);
    if (encoded) body[bodyKey] = encoded;
  }
  return body;
}

export function extractImageBase64(jobState) {
  if (!jobState || typeof jobState !== "object") return "";
  if (typeof jobState.b64_json === "string" && jobState.b64_json) return jobState.b64_json;
  const result = jobState.result;
  if (result && typeof result === "object") {
    if (typeof result.b64_json === "string" && result.b64_json) return result.b64_json;
    const images = Array.isArray(result.images) ? result.images : [];
    if (images.length && typeof images[0]?.b64_json === "string") return images[0].b64_json;
  }
  return "";
}

export function startServer(options = {}) {
  const args = buildServerArgs(options);
  const [command, ...commandArgs] = args;
  return spawn(command, commandArgs, {
    cwd: options.cwd || path.dirname(command),
    stdio: options.logStream ? ["ignore", options.logStream, options.logStream] : "ignore",
    windowsHide: true
  });
}

export async function waitForServer(options = {}) {
  const host = options.host || "127.0.0.1";
  const port = options.port || 1234;
  const timeoutMs = Number(options.timeoutMs || 120000);
  const deadline = Date.now() + timeoutMs;
  let lastError = "";

  while (Date.now() < deadline) {
    try {
      const response = await fetch(`http://${host}:${port}/sdcpp/v1/capabilities`);
      if (response.ok) return true;
      lastError = `${response.status} ${await response.text()}`;
    } catch (error) {
      lastError = error.message;
    }
    await sleep(Number(options.pollMs || 500));
  }

  throw new TcxSdError(`sd-server did not become ready: ${lastError}`, { code: errorCodes.SERVER_START_FAILED });
}

async function readJsonResponse(response, prefix) {
  if (!response.ok) {
    const text = await response.text();
    throw new TcxSdError(`${prefix}: ${response.status} ${text}`, { details: { status: response.status, body: text } });
  }
  return response.json();
}

export async function submitImageJob(options = {}) {
  const host = options.host || "127.0.0.1";
  const port = options.port || 1234;
  const body = options.body || buildImageRequest(options);
  const response = await fetch(`http://${host}:${port}/sdcpp/v1/img_gen`, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify(body)
  });
  return readJsonResponse(response, "sd-server submit failed");
}

export async function cancelImageJob(job, options = {}) {
  const host = options.host || "127.0.0.1";
  const port = options.port || 1234;
  const pollUrl = job?.poll_url || (job?.id ? `/sdcpp/v1/jobs/${job.id}` : "");
  if (!pollUrl) {
    throw new TcxSdError("Cannot cancel sd-server job without id or poll_url", { code: errorCodes.CANCEL_NOT_INTERRUPTIBLE });
  }
  const response = await fetch(`http://${host}:${port}${pollUrl}/cancel`, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: "{}"
  });
  return readJsonResponse(response, "sd-server cancel failed");
}

export async function pollImageJob(job, options = {}) {
  const host = options.host || "127.0.0.1";
  const port = options.port || 1234;
  const pollUrl = job.poll_url || `/sdcpp/v1/jobs/${job.id}`;
  const timeoutMs = Number(options.timeoutMs || 300000);
  const deadline = Date.now() + timeoutMs;

  while (Date.now() < deadline) {
    if (options.signal?.aborted) {
      await cancelImageJob({ ...job, poll_url: pollUrl }, options);
      throw new TcxSdError("sd-server job cancellation requested; active generation may not interrupt immediately.", {
        code: errorCodes.CANCEL_NOT_INTERRUPTIBLE
      });
    }
    const response = await fetch(`http://${host}:${port}${pollUrl}`);
    const state = await readJsonResponse(response, "sd-server poll failed");
    if (["completed", "failed", "cancelled"].includes(state.status)) return state;
    await sleep(Number(options.pollMs || 500));
  }

  await cancelImageJob({ ...job, poll_url: pollUrl }, options).catch(() => undefined);
  throw new TcxSdError("sd-server job timed out", { code: errorCodes.TIMEOUT });
}

export function assessResultQuality(sidecar) {
  const metadata = sidecar?.metadata || {};
  const errors = [];
  const warnings = [];
  const prompt = String(metadata.prompt || "");
  if (["TODO", "TBD", "{subject}", "{prompt}", "{{", "}}", "__"].some((marker) => prompt.includes(marker))) {
    warnings.push("PLACEHOLDER_PROMPT");
  }
  const expectedWidth = Number(metadata.width || 0);
  const expectedHeight = Number(metadata.height || 0);
  const actualWidth = Number(sidecar?.image_width || 0);
  const actualHeight = Number(sidecar?.image_height || 0);
  if (expectedWidth && expectedHeight && actualWidth && actualHeight && (expectedWidth !== actualWidth || expectedHeight !== actualHeight)) {
    errors.push("SIZE_MISMATCH");
  }
  if ((metadata.visible_text || metadata.expected_text) && !["true", "1", "yes", "passed"].includes(String(metadata.text_verified || "").toLowerCase())) {
    warnings.push("TEXT_NOT_VERIFIED");
  }
  return {
    ok: errors.length === 0 && sidecar?.ok !== false,
    error_codes: errors,
    warning_codes: warnings
  };
}

export function buildSidecar({ ok, status, error = "", outputPath = "", options = {}, completed = {}, submitted = {} } = {}) {
  const modelId = options.model || "ideogram4-q4_0";
  const defaults = qualityDefaults(modelId, options.quality);
  const metadata = {
    ...(options.metadata || {}),
    prompt: options.prompt || "",
    negative_prompt: options.negativePrompt || "",
    width: String(options.width || defaults.width),
    height: String(options.height || defaults.height),
    steps: String(options.steps || defaults.steps),
    seed: String(options.seed ?? -1),
    cfg_scale: String(options.cfgScale || defaults.cfgScale),
    sampler: String(options.sampler || defaults.sampler || "euler"),
    request_mode: options.requestMode || "text_to_image",
    execution_mode: "persistent_server",
    runtime_execution_mode: "persistent_server",
    model: modelId,
    model_family: profileFor(modelId).family,
    backend: options.backend || runtimeDefaults(modelId, options).backend || "auto"
  };
  for (const [optionKey, metadataKey] of [
    ["initImage", "init_image"],
    ["maskImage", "mask_image"],
    ["controlImage", "control_image"],
    ["sourceImage", "source_image"]
  ]) {
    if (options[optionKey]) metadata[metadataKey] = String(options[optionKey]);
  }
  if (options.controlStrength !== undefined) metadata.control_strength = String(options.controlStrength);
  if (options.strength !== undefined) metadata.strength = String(options.strength);
  if (options.upscaleFactor !== undefined) metadata.upscale_factor = String(options.upscaleFactor);
  if (Array.isArray(options.loras) && options.loras.length) metadata.lora_count = String(options.loras.length);
  const sidecar = {
    job_id: Number(options.jobId || 1),
    job_label: path.basename(outputPath || options.output || "node_job", path.extname(outputPath || options.output || "node_job")),
    ok: Boolean(ok),
    state: ok ? "complete" : status === "cancelled" ? "cancelled" : "failed",
    error: String(error || ""),
    duration_seconds: Number(options.durationSeconds || 0),
    native_output_path: outputPath,
    saved_image_path: ok ? outputPath : undefined,
    image_width: completed.image_width || options.width || defaults.width,
    image_height: completed.image_height || options.height || defaults.height,
    sidecar_path: options.sidecarPath,
    metadata
  };
  if (submitted.id || completed.id) sidecar.metadata.server_job_id = String(completed.id || submitted.id);
  if (error) {
    const payload = errorPayload(error);
    sidecar.error_code = payload.code;
    sidecar.remediation_hints = payload.remediation_hints;
  }
  sidecar.quality = assessResultQuality(sidecar);
  return Object.fromEntries(Object.entries(sidecar).filter(([, value]) => value !== undefined));
}

export async function writeSidecar(sidecarPath, sidecar) {
  const absolute = path.resolve(sidecarPath);
  await mkdir(path.dirname(absolute), { recursive: true });
  await writeFile(absolute, JSON.stringify({ ...sidecar, sidecar_path: absolute }, null, 2), "utf8");
  return absolute;
}

export class TcxSdServerSession {
  constructor(options = {}) {
    this.options = { ...options, manageServer: false };
    this.child = null;
  }

  async start() {
    if (!this.options.reuseServer) {
      this.child = startServer(this.options);
    }
    await waitForServer(this.options);
    return this;
  }

  async generate(options = {}) {
    return runTextToImage({ ...this.options, ...options, manageServer: false });
  }

  close() {
    if (this.child) {
      this.child.kill();
      this.child = null;
    }
  }
}

export function createServerSession(options = {}) {
  return new TcxSdServerSession(options);
}

export class GenerationProject {
  constructor(options = {}) {
    const root = path.resolve(options.root || defaultExampleRoot);
    this.name = options.name || "tcxsd-project";
    this.root = path.join(root, this.name);
    this.outputRoot = path.resolve(options.outputRoot || path.join(this.root, "outputs"));
    this.tempRoot = path.resolve(options.tempRoot || path.join(this.root, "tmp"));
    this.cacheRoot = path.resolve(options.cacheRoot || path.join(this.root, "cache"));
    this.logRoot = path.resolve(options.logRoot || path.join(this.root, "logs"));
    this.inputRoot = path.resolve(options.inputRoot || path.join(this.root, "inputs"));
  }

  storageOptions() {
    return {
      outputRoot: this.outputRoot,
      tempRoot: this.tempRoot,
      cacheRoot: this.cacheRoot
    };
  }

  outputPath(label, extension = ".png") {
    const suffix = extension.startsWith(".") ? extension : `.${extension}`;
    return path.join(this.outputRoot, `${label}${suffix}`);
  }

  sidecarPath(label) {
    return path.join(this.outputRoot, `${label}.json`);
  }

  artifact(label, metadata = {}) {
    return new GenerationArtifact({
      id: label,
      outputPath: this.outputPath(label),
      sidecarPath: this.sidecarPath(label),
      metadata
    });
  }
}

export class GenerationArtifact {
  constructor({ id = "", outputPath = "", sidecarPath = "", parentSidecarPath = "", metadata = {} } = {}) {
    this.id = id;
    this.outputPath = outputPath;
    this.sidecarPath = sidecarPath;
    this.parentSidecarPath = parentSidecarPath;
    this.metadata = { ...metadata };
  }

  static fromResult(result = {}, sidecar = {}) {
    return new GenerationArtifact({
      id: String(result.serverJobId || sidecar.job_id || path.basename(result.outputPath || "artifact", path.extname(result.outputPath || ""))),
      outputPath: result.outputPath || sidecar.saved_image_path || "",
      sidecarPath: result.sidecarPath || sidecar.sidecar_path || "",
      metadata: sidecar.metadata || {}
    });
  }
}

export function createGenerationProject(options = {}) {
  return new GenerationProject(options);
}

export class GenerationSession {
  constructor(options = {}) {
    const model = options.model || "ideogram4-q4_0";
    this.id = options.id || model;
    this.model = model;
    this.profile = profileFor(model);
    this.runtimePreset = options.runtimePreset || "default";
    this.project = options.project instanceof GenerationProject
      ? options.project
      : createGenerationProject({
        root: options.projectRoot || options.root,
        name: options.projectName || options.name || model,
        outputRoot: options.outputRoot,
        tempRoot: options.tempRoot,
        cacheRoot: options.cacheRoot,
        logRoot: options.logRoot,
        inputRoot: options.inputRoot
      });
    this.options = {
      ...this.project.storageOptions(),
      ...options,
      model,
      runtimePreset: this.runtimePreset,
      project: this.project
    };
    this.capabilities = getBackendCapabilities({
      executionMode: options.executionMode || "persistent_server",
      model,
      hasControlNet: Boolean(this.profile.assets.control_net),
      loraModelDir: options.loraModelDir
    });
    this.server = null;
  }

  request(quality = "balanced", options = {}) {
    return createTextToImageRequest({
      ...this.options,
      quality,
      ...options,
      metadata: {
        ...(this.options.metadata || {}),
        ...(options.metadata || {}),
        generation_session: this.id,
        model_profile: this.model,
        runtime_preset: this.runtimePreset,
        project_root: this.project.root
      }
    });
  }

  artifact(label, metadata = {}) {
    return this.project.artifact(label, metadata);
  }

  supports(request) {
    try {
      assertRequestSupported(request, {
        executionMode: this.options.executionMode || "persistent_server",
        model: this.model,
        hasControlNet: Boolean(this.profile.assets.control_net),
        loraModelDir: this.options.loraModelDir
      });
      return true;
    } catch (error) {
      if (error instanceof TcxSdError && error.code === errorCodes.BACKEND_UNSUPPORTED) {
        return false;
      }
      throw error;
    }
  }

  unsupportedReason(request) {
    try {
      assertRequestSupported(request, {
        executionMode: this.options.executionMode || "persistent_server",
        model: this.model,
        hasControlNet: Boolean(this.profile.assets.control_net),
        loraModelDir: this.options.loraModelDir
      });
      return "";
    } catch (error) {
      return error?.message || String(error);
    }
  }

  serverSession(options = {}) {
    return createServerSession({ ...this.options, ...options });
  }

  async start(options = {}) {
    this.server = await this.serverSession(options).start();
    return this;
  }

  async generate(options = {}) {
    if (this.server) {
      return this.server.generate({ ...this.options, ...options });
    }
    return runTextToImage({ ...this.options, ...options });
  }

  async runBatch(batch, options = {}) {
    return runBatchJob(batch, { ...this.options, project: this.project, ...options });
  }

  close() {
    if (this.server) {
      this.server.close();
      this.server = null;
    }
  }
}

export function createGenerationSession(options = {}) {
  return new GenerationSession(options);
}

export class BatchJob {
  constructor(options = "batch") {
    const config = typeof options === "string" ? { label: options } : { ...options };
    this.label = config.label || "batch";
    this.baseRequest = config.baseRequest ? { ...config.baseRequest } : null;
    this.requests = [];
    for (const request of config.requests || []) {
      this.add(request);
    }
  }

  add(request) {
    this.requests.push({
      ...request,
      metadata: {
        ...(request.metadata || {}),
        batch_label: this.label,
        batch_index: String(this.requests.length)
      }
    });
    return this;
  }

  seedSweep(seeds = [], baseRequest = this.baseRequest) {
    if (!baseRequest) {
      throw new TcxSdError("seedSweep requires a baseRequest", { code: errorCodes.MODEL_ASSET_MISSING });
    }
    for (const seed of seeds) {
      this.add({
        ...baseRequest,
        seed,
        metadata: {
          ...(baseRequest.metadata || {}),
          batch_kind: "seed_sweep",
          batch_seed: String(seed)
        }
      });
    }
    return this;
  }
}

export function createBatchJob(options = "batch") {
  return new BatchJob(options);
}

export function createVariantJob(artifact, options = {}) {
  const request = createImageToImageRequest({
    ...options,
    initImage: options.initImage || artifact.outputPath,
    strength: options.strength ?? 0.55,
    metadata: {
      ...(options.metadata || {}),
      variant_source: artifact.id || "",
      parent_sidecar_path: artifact.sidecarPath || ""
    }
  });
  return { artifact, request };
}

export async function runBatchJob(batch, options = {}) {
  const results = [];
  for (const [index, request] of batch.requests.entries()) {
    const label = `${batch.label.replace(/[^a-z0-9_-]+/gi, "_")}_${index}`;
    results.push(await runTextToImage({
      ...options,
      ...request,
      output: request.output || (options.project ? options.project.outputPath(label) : undefined),
      sidecar: request.sidecar || (options.project ? options.project.sidecarPath(label) : undefined)
    }));
  }
  return results;
}

export async function runTextToImage(options = {}) {
  let child = null;
  const manageServer = options.manageServer !== false;
  const started = Date.now();
  try {
    assertRequestSupported(options, {
      executionMode: "persistent_server",
      model: options.model,
      hasControlNet: Boolean(profileFor(options.model || "ideogram4-q4_0").assets.control_net),
      loraModelDir: options.loraModelDir
    });
    if (manageServer) {
      child = startServer(options);
      await waitForServer(options);
    }
    const submitted = await submitImageJob(options);
    const completed = await pollImageJob(submitted, options);
    if (completed.status !== "completed") {
      throw new TcxSdError(`sd-server job ${completed.status}: ${JSON.stringify(completed.error || completed)}`);
    }
    const encoded = extractImageBase64(completed);
    if (!encoded) {
      throw new TcxSdError("sd-server completed without b64_json output", { code: errorCodes.OUTPUT_MISSING });
    }
    const roots = resolveStorageRoots(options);
    const outputPath = path.resolve(options.output || path.join(roots.outputRoot, `${Date.now()}.png`));
    await mkdir(path.dirname(outputPath), { recursive: true });
    await writeFile(outputPath, Buffer.from(encoded, "base64"));
    const pngSize = readPngSize(outputPath);
    const sidecarPath = path.resolve(options.sidecar || outputPath.replace(/\.[^.]+$/, ".json"));
    const sidecar = buildSidecar({
      ok: true,
      status: completed.status,
      outputPath,
      options: { ...options, sidecarPath, durationSeconds: (Date.now() - started) / 1000 },
      completed: {
        ...completed,
        image_width: pngSize?.width,
        image_height: pngSize?.height
      },
      submitted
    });
    await writeSidecar(sidecarPath, sidecar);
    return {
      ok: true,
      outputPath,
      sidecarPath,
      status: completed.status,
      serverJobId: completed.id || submitted.id || ""
    };
  } catch (error) {
    const tcxError = error instanceof TcxSdError
      ? error
      : new TcxSdError(error?.message || String(error), { cause: error });
    const roots = resolveStorageRoots(options);
    const outputPath = path.resolve(options.output || path.join(roots.outputRoot, `${Date.now()}_failed.png`));
    const sidecarPath = path.resolve(options.sidecar || outputPath.replace(/\.[^.]+$/, ".json"));
    const sidecar = buildSidecar({
      ok: false,
      status: tcxError.code === errorCodes.CANCEL_NOT_INTERRUPTIBLE ? "cancelled" : "failed",
      error: tcxError.message,
      outputPath,
      options: { ...options, sidecarPath, durationSeconds: (Date.now() - started) / 1000 }
    });
    try {
      await writeSidecar(sidecarPath, sidecar);
      tcxError.details = { ...(tcxError.details || {}), sidecarPath };
    } catch (sidecarError) {
      tcxError.details = {
        ...(tcxError.details || {}),
        sidecarError: sidecarError?.message || String(sidecarError)
      };
    }
    throw tcxError;
  } finally {
    if (child) child.kill();
  }
}

export async function runJsonJob(jobPath, overrides = {}) {
  const absoluteJob = path.resolve(jobPath);
  const job = JSON.parse(await readFile(absoluteJob, "utf8"));
  const jobDir = path.dirname(absoluteJob);
  const outputDir = job.output_dir
    ? path.resolve(jobDir, job.output_dir)
    : path.join(defaultExampleRoot, "outputs", "node");
  const runtime = job.runtime || {};
  return runTextToImage({
    model: job.model,
    modelDir: job.model_dir ? path.resolve(jobDir, job.model_dir) : undefined,
    nativeDir: job.native_dir ? path.resolve(jobDir, job.native_dir) : undefined,
    output: path.join(outputDir, `${job.output_name || "node_job"}.png`),
    sidecar: path.join(outputDir, `${job.output_name || "node_job"}.json`),
    prompt: job.prompt || JSON.stringify(job.prompt_json || {}),
    negativePrompt: job.negative_prompt || "",
    quality: job.quality,
    width: job.width,
    height: job.height,
    steps: job.steps,
    seed: job.seed,
    cfgScale: job.cfg_scale,
    sampler: job.sampler,
    runtimePreset: runtime.preset || runtime.profile,
    backend: runtime.backend,
    paramsBackend: runtime.params_backend,
    offloadToCpu: runtime.offload_to_cpu,
    diffusionFlashAttention: runtime.diffusion_flash_attention,
    mmap: runtime.mmap,
    streamLayers: runtime.stream_layers,
    maxVramGiB: runtime.max_vram_gib,
    initImage: job.init_image ? path.resolve(jobDir, job.init_image) : undefined,
    maskImage: job.mask_image ? path.resolve(jobDir, job.mask_image) : undefined,
    controlImage: job.control_image ? path.resolve(jobDir, job.control_image) : undefined,
    loras: job.loras,
    ...overrides
  });
}

export const promptPacks = {
  ideogram4Poster({ subject, visibleText, language = "en" } = {}) {
    const zh = String(language).toLowerCase().startsWith("zh");
    const text = visibleText || "tcxStableDiffusion";
    const highLevel = zh
      ? `${subject || "一张高级中文海报"}。画面中必须包含清晰可读的文字“${text}”。整体干净、精致、适合展示。`
      : `${subject || "A premium poster"}. Include the exact readable text "${text}".`;
    return {
      prompt_json: {
        high_level_description: highLevel,
        style_description: {
          aesthetics: zh
            ? "中文海报设计，高级编辑排版，清晰层级，干净背景，精致商业视觉"
            : "premium editorial poster design, crisp layout, refined typography",
          lighting: "clear controlled lighting with readable contrast",
          medium: "local AI image generation with polished commercial-art direction",
          color_palette: ["#F4F1EA", "#111111", "#2F80ED", "#27AE60", "#FFFFFF"]
        },
        compositional_deconstruction: {
          canvas: "upright image canvas; do not rotate the image or any text",
          background: "clean intentional background that supports the subject without clutter",
          layout: zh
            ? "海报构图，主体明确，文字水平摆放，从左到右可读，边距均衡"
            : "upright poster layout with readable horizontal text and balanced margins",
          elements: [
            { type: "obj", desc: subject || "" },
            {
              type: "text",
              desc: zh
                ? `只打印准确文字“${text}”，保持水平、端正、完整、无错字、无镜像、无额外字符。`
                : `Print the exact text "${text}" horizontally, upright, and readable left to right.`
            }
          ]
        }
      },
      negative_prompt: zh
        ? "低质量，模糊，错别字，不可读文字，镜像文字，旋转文字，裁切文字，水印，签名"
        : "low quality, blurry, misspelled text, unreadable text, cropped text, mirrored text, watermark, signature",
      metadata: {
        prompt_profile: "ideogram4",
        prompt_kind: "poster",
        visible_text: text,
        language: zh ? "zh" : "en"
      }
    };
  },
  productShot({ subject = "a local AI image generation product", language = "en" } = {}) {
    const zh = String(language).toLowerCase().startsWith("zh");
    return {
      prompt: zh
        ? `${subject}，干净产品摄影，受控棚拍光线，清晰材质，商业展示级构图`
        : `${subject}, clean product photography, controlled studio lighting, crisp materials, commercial catalog quality`,
      negative_prompt: zh ? "低质量，模糊，杂乱，水印，签名" : "low quality, blurry, clutter, watermark, signature",
      metadata: { prompt_pack: "product_shot", language: zh ? "zh" : "en" }
    };
  },
  wideScene({ subject = "a cinematic local creative studio", language = "en" } = {}) {
    const zh = String(language).toLowerCase().startsWith("zh");
    return {
      prompt: zh
        ? `${subject}，宽幅电影构图，主体层级清楚，色彩精致，氛围明确但不杂乱`
        : `${subject}, cinematic wide composition, readable subject hierarchy, refined color, atmospheric but clear`,
      negative_prompt: zh ? "低质量，模糊，透视扭曲，杂乱，水印，签名" : "low quality, blurry, distorted perspective, clutter, watermark, signature",
      metadata: { prompt_pack: "wide_scene", language: zh ? "zh" : "en" }
    };
  },
  gameAsset({ subject = "a readable game asset", language = "en" } = {}) {
    const zh = String(language).toLowerCase().startsWith("zh");
    return {
      prompt: zh
        ? `${subject}，清晰轮廓，适合游戏制作，形体明确，材质一致，干净背景`
        : `${subject}, isolated readable silhouette, production-ready game art, clean shape language, consistent material detail`,
      negative_prompt: zh ? "低质量，模糊，轮廓混乱，多余肢体，水印，签名" : "low quality, blurry, messy silhouette, extra limbs, watermark, signature",
      metadata: { prompt_pack: "game_asset", language: zh ? "zh" : "en" }
    };
  },
  uiMockup({ subject = "a professional software UI mockup", language = "en" } = {}) {
    const zh = String(language).toLowerCase().startsWith("zh");
    return {
      prompt: zh
        ? `${subject}，专业软件界面，信息分组清楚，面板有序，克制视觉设计`
        : `${subject}, professional interface mockup, organized panels, readable layout, restrained visual design`,
      negative_prompt: zh ? "低质量，不可读文字，杂乱，变形界面，水印，签名" : "low quality, unreadable text, clutter, distorted UI, watermark, signature",
      metadata: { prompt_pack: "ui_mockup", language: zh ? "zh" : "en" }
    };
  }
};

export const canvasPresets = {
  squarePreview: { width: 512, height: 512, label: "square_preview" },
  mobilePoster: { width: 768, height: 1344, label: "mobile_poster" },
  wideHero: { width: 1280, height: 720, label: "wide_hero" },
  desktopWallpaper: { width: 1536, height: 864, label: "desktop_wallpaper" },
  appIcon: { width: 1024, height: 1024, label: "app_icon" }
};

export const stylePresets = {
  commercialPoster: { promptPack: "ideogram4Poster", preferredModel: "ideogram4-q4_0" },
  cleanProductShot: { promptPack: "productShot", preferredModel: "flux2-klein-4b-q4_0" },
  wideScene: { promptPack: "wideScene", preferredModel: "z-image-turbo-q3_k" },
  gameAsset: { promptPack: "gameAsset", preferredModel: "flux2-klein-4b-q4_0" },
  uiMockup: { promptPack: "uiMockup", preferredModel: "flux2-klein-4b-q4_0" },
  controlNetCanny: { promptPack: "wideScene", preferredModel: "sd15-controlnet-canny" }
};

export function routeModelForIntent(intent = "cleanProductShot", options = {}) {
  if (options.model) return options.model;
  if (options.controlImage || intent === "controlNetCanny") return "sd15-controlnet-canny";
  if (intent === "commercialPoster" || intent === "poster" || options.visibleText) return "ideogram4-q4_0";
  if (intent === "wideScene" || intent === "wide") return "z-image-turbo-q3_k";
  return stylePresets[intent]?.preferredModel || "flux2-klein-4b-q4_0";
}
