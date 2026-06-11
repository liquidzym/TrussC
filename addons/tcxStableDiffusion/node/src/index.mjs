import { Buffer } from "node:buffer";
import { spawn } from "node:child_process";
import { mkdir, readFile, writeFile } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const moduleDir = path.dirname(fileURLToPath(import.meta.url));

export const addonRoot = path.resolve(moduleDir, "..", "..");
export const defaultExampleRoot = path.join(addonRoot, "examples", "ideogram4-basic");
export const defaultNativeDir = path.join(addonRoot, "libs", "stable-diffusion", "current");
export const defaultModelRoot = path.join(defaultExampleRoot, "data", "models");

export const modelProfiles = {
  "ideogram4-q4_0": {
    family: "Ideogram4",
    width: 1024,
    height: 1024,
    steps: 8,
    cfgScale: 1.0,
    assets: {
      diffusion: "ideogram4-Q4_0.gguf",
      uncond_diffusion: "ideogram4_uncond-Q4_0.gguf",
      llm: "Qwen3VL-8B-Instruct-Q4_K_M.gguf",
      vae: "flux2_ae.safetensors"
    }
  },
  "flux2-klein-4b-q4_0": {
    family: "FLUX.2-klein",
    width: 512,
    height: 512,
    steps: 4,
    cfgScale: 1.0,
    assets: {
      diffusion: "flux-2-klein-4b-Q4_0.gguf",
      llm: "Qwen3-4B-Q4_K_M.gguf",
      vae: "flux2_ae.safetensors"
    }
  },
  "z-image-turbo-q3_k": {
    family: "Z-Image",
    width: 1024,
    height: 512,
    steps: 8,
    cfgScale: 1.0,
    assets: {
      diffusion: "z_image_turbo-Q3_K.gguf",
      llm: "Qwen3-4B-Instruct-2507-Q4_K_M.gguf",
      vae: "z_image_ae.safetensors"
    }
  }
};

const roleArgs = {
  diffusion: "--diffusion-model",
  uncond_diffusion: "--uncond-diffusion-model",
  llm: "--llm",
  vae: "--vae"
};

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

export function resolveModelDir(modelId, options = {}) {
  if (options.modelDir) {
    return path.resolve(options.modelDir);
  }
  if (!modelProfiles[modelId]) {
    throw new Error(`Unknown model '${modelId}'. Known models: ${Object.keys(modelProfiles).join(", ")}`);
  }
  return path.join(options.modelRoot || defaultModelRoot, modelId);
}

export function resolveServerExecutable(options = {}) {
  if (options.serverExecutable) {
    return path.resolve(options.serverExecutable);
  }
  const exe = process.platform === "win32" ? "sd-server.exe" : "sd-server";
  return path.join(options.nativeDir || defaultNativeDir, "bin", exe);
}

export function buildServerArgs(options = {}) {
  const modelId = options.model || "ideogram4-q4_0";
  const profile = modelProfiles[modelId];
  if (!profile) {
    throw new Error(`Unknown model '${modelId}'.`);
  }

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

  if (options.backend) {
    args.push("--backend", options.backend);
  }
  if (options.paramsBackend) {
    args.push("--params-backend", options.paramsBackend);
  }
  if (options.offloadToCpu) {
    args.push("--offload-to-cpu");
  }
  if (options.diffusionFlashAttention !== false) {
    args.push("--diffusion-fa");
  }
  if (options.mmap !== false) {
    args.push("--mmap");
  }
  args.push("-v");
  return args;
}

export function buildImageRequest(options = {}) {
  const modelId = options.model || "ideogram4-q4_0";
  const profile = modelProfiles[modelId];
  if (!profile) {
    throw new Error(`Unknown model '${modelId}'.`);
  }

  return {
    prompt: options.prompt || "",
    negative_prompt: options.negativePrompt || "",
    width: Number(options.width || profile.width),
    height: Number(options.height || profile.height),
    strength: Number(options.strength || 0.75),
    seed: Number(options.seed ?? -1),
    batch_count: 1,
    control_strength: Number(options.controlStrength || 1.0),
    sample_params: {
      sample_method: options.sampler || "euler",
      sample_steps: Number(options.steps || profile.steps),
      guidance: {
        txt_cfg: Number(options.cfgScale || profile.cfgScale)
      }
    },
    lora: [],
    output_format: "png",
    output_compression: 100
  };
}

export function extractImageBase64(jobState) {
  if (!jobState || typeof jobState !== "object") {
    return "";
  }
  if (typeof jobState.b64_json === "string" && jobState.b64_json) {
    return jobState.b64_json;
  }
  const result = jobState.result;
  if (result && typeof result === "object") {
    if (typeof result.b64_json === "string" && result.b64_json) {
      return result.b64_json;
    }
    const images = Array.isArray(result.images) ? result.images : [];
    if (images.length && typeof images[0]?.b64_json === "string") {
      return images[0].b64_json;
    }
  }
  return "";
}

export function startServer(options = {}) {
  const args = buildServerArgs(options);
  const [command, ...commandArgs] = args;
  const child = spawn(command, commandArgs, {
    cwd: options.cwd || path.dirname(command),
    stdio: options.logStream ? ["ignore", options.logStream, options.logStream] : "ignore",
    windowsHide: true
  });
  return child;
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
      if (response.ok) {
        return true;
      }
      lastError = `${response.status} ${await response.text()}`;
    } catch (error) {
      lastError = error.message;
    }
    await sleep(Number(options.pollMs || 500));
  }

  throw new Error(`sd-server did not become ready: ${lastError}`);
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
  if (!response.ok) {
    throw new Error(`sd-server submit failed: ${response.status} ${await response.text()}`);
  }
  return response.json();
}

export async function pollImageJob(job, options = {}) {
  const host = options.host || "127.0.0.1";
  const port = options.port || 1234;
  const pollUrl = job.poll_url || `/sdcpp/v1/jobs/${job.id}`;
  const timeoutMs = Number(options.timeoutMs || 300000);
  const deadline = Date.now() + timeoutMs;

  while (Date.now() < deadline) {
    const response = await fetch(`http://${host}:${port}${pollUrl}`);
    if (!response.ok) {
      throw new Error(`sd-server poll failed: ${response.status} ${await response.text()}`);
    }
    const state = await response.json();
    if (["completed", "failed", "cancelled"].includes(state.status)) {
      return state;
    }
    await sleep(Number(options.pollMs || 500));
  }

  throw new Error("sd-server job timed out");
}

export async function runTextToImage(options = {}) {
  let child = null;
  const manageServer = options.manageServer !== false;
  if (manageServer) {
    child = startServer(options);
    await waitForServer(options);
  }

  try {
    const submitted = await submitImageJob(options);
    const completed = await pollImageJob(submitted, options);
    if (completed.status !== "completed") {
      throw new Error(`sd-server job ${completed.status}: ${JSON.stringify(completed.error || completed)}`);
    }
    const encoded = extractImageBase64(completed);
    if (!encoded) {
      throw new Error("sd-server completed without b64_json output");
    }
    const outputPath = path.resolve(options.output || path.join(defaultExampleRoot, "outputs", "node", `${Date.now()}.png`));
    await mkdir(path.dirname(outputPath), { recursive: true });
    await writeFile(outputPath, Buffer.from(encoded, "base64"));
    return {
      ok: true,
      outputPath,
      status: completed.status,
      serverJobId: completed.id || submitted.id || ""
    };
  } finally {
    if (child) {
      child.kill();
    }
  }
}

export async function runJsonJob(jobPath, overrides = {}) {
  const absoluteJob = path.resolve(jobPath);
  const job = JSON.parse(await readFile(absoluteJob, "utf8"));
  const jobDir = path.dirname(absoluteJob);
  const outputDir = job.output_dir
    ? path.resolve(jobDir, job.output_dir)
    : path.join(defaultExampleRoot, "outputs", "node");
  return runTextToImage({
    model: job.model,
    modelDir: job.model_dir ? path.resolve(jobDir, job.model_dir) : undefined,
    nativeDir: job.native_dir ? path.resolve(jobDir, job.native_dir) : undefined,
    output: path.join(outputDir, `${job.output_name || "node_job"}.png`),
    prompt: job.prompt || JSON.stringify(job.prompt_json || {}),
    negativePrompt: job.negative_prompt || "",
    width: job.width,
    height: job.height,
    steps: job.steps,
    seed: job.seed,
    cfgScale: job.cfg_scale,
    backend: job.runtime?.backend || "cuda0",
    paramsBackend: job.runtime?.params_backend || "cpu",
    offloadToCpu: job.runtime?.offload_to_cpu !== false,
    diffusionFlashAttention: job.runtime?.diffusion_flash_attention !== false,
    ...overrides
  });
}
