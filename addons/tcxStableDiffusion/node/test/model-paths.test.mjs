import assert from "node:assert/strict";
import { mkdir, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { existsSync } from "node:fs";
import os from "node:os";
import path from "node:path";
import test from "node:test";
import {
  TcxSdError,
  assessResultQuality,
  buildImageRequest,
  buildServerArgs,
  buildSidecar,
  cancelImageJob,
  cleanupStorage,
  defaultExampleRoot,
  defaultModelRoot,
  extractImageBase64,
  modelProfiles,
  promptPacks,
  runTextToImage,
  resolveStorageRoots,
  resolveModelDir
} from "../src/index.mjs";

test("default model directory uses the shared example bin data folder", () => {
  assert.equal(defaultModelRoot, path.join(defaultExampleRoot, "bin", "data", "models"));
  assert.equal(resolveModelDir("ideogram4-q4_0"), path.join(defaultModelRoot, "ideogram4-q4_0"));
});

test("server args map model assets to sd-server flags", () => {
  const args = buildServerArgs({
    model: "flux2-klein-4b-q4_0",
    nativeDir: "N:/native",
    modelRoot: "M:/models",
    backend: "cuda0",
    paramsBackend: "cpu",
    offloadToCpu: true
  }).join("\n");

  assert.match(args, /sd-server/);
  assert.match(args, /--diffusion-model/);
  assert.match(args, /flux-2-klein-4b-Q4_0\.gguf/);
  assert.match(args, /--llm/);
  assert.match(args, /Qwen3-4B-Q4_K_M\.gguf/);
  assert.match(args, /--vae/);
  assert.match(args, /--backend\ncuda0/);
  assert.match(args, /--params-backend\ncpu/);
  assert.match(args, /--offload-to-cpu/);
});

test("image request uses model defaults with clear override names", () => {
  const body = buildImageRequest({
    model: "z-image-turbo-q3_k",
    prompt: "wide studio",
    seed: 4096
  });

  assert.equal(body.prompt, "wide studio");
  assert.equal(body.width, 1024);
  assert.equal(body.height, 512);
  assert.equal(body.sample_params.sample_steps, 8);
  assert.equal(body.sample_params.guidance.txt_cfg, 1);
  assert.equal(body.seed, 4096);
});

test("extracts image payload from native sd-server job result shape", () => {
  const encoded = extractImageBase64({
    status: "completed",
    result: {
      images: [
        { index: 0, b64_json: "aGVsbG8=" }
      ]
    }
  });

  assert.equal(encoded, "aGVsbG8=");
});

test("model profiles expose per-model quality and runtime presets", () => {
  assert.equal(modelProfiles["ideogram4-q4_0"].quality.balanced.width, 1024);
  assert.equal(modelProfiles["ideogram4-q4_0"].quality.balanced.cfgScale, 7);
  assert.equal(modelProfiles["flux2-klein-4b-q4_0"].quality.draft.steps, 4);
  assert.equal(modelProfiles["z-image-turbo-q3_k"].runtime.lowVram.paramsBackend, "cpu");
  assert.equal(modelProfiles["ideogram4-q4_0"].runtime.rtx4090FullSpeed.paramsBackend, "cuda0");
});

test("image request maps image inputs loras and Chinese prompt text", () => {
  const dataUrl = "data:image/png;base64,aGVsbG8=";
  const body = buildImageRequest({
    model: "ideogram4-q4_0",
    quality: "draft",
    prompt: "一张中文海报",
    initImage: dataUrl,
    maskImage: dataUrl,
    controlImage: dataUrl,
    loras: [{ path: "poster.safetensors", weight: 0.65 }],
    controlStrength: 0.8
  });

  assert.equal(body.prompt, "一张中文海报");
  assert.equal(body.width, 512);
  assert.equal(body.init_image, dataUrl);
  assert.equal(body.mask_image, dataUrl);
  assert.equal(body.control_image, dataUrl);
  assert.equal(body.control_strength, 0.8);
  assert.equal(body.lora[0].path, "poster.safetensors");
  assert.equal(body.lora[0].multiplier, 0.65);
});

test("runtime preset feeds server args without overwriting explicit backend", () => {
  const args = buildServerArgs({
    model: "ideogram4-q4_0",
    nativeDir: "N:/native",
    modelRoot: "M:/models",
    runtimePreset: "lowVram",
    backend: "cuda1"
  });

  assert.deepEqual(args.slice(args.indexOf("--backend"), args.indexOf("--backend") + 2), ["--backend", "cuda1"]);
  assert.match(args.join("\n"), /--params-backend\ncpu/);
  assert.ok(args.includes("--offload-to-cpu"));
  assert.ok(args.includes("--stream-layers"));
});

test("sidecar parity includes error code remediation and quality report", () => {
  const sidecar = buildSidecar({
    ok: false,
    status: "failed",
    error: "CUDA out of memory",
    outputPath: "out.png",
    options: {
      model: "ideogram4-q4_0",
      prompt: "TODO {subject}",
      width: 512,
      height: 512,
      seed: 9
    }
  });

  assert.equal(sidecar.ok, false);
  assert.equal(sidecar.error_code, "CUDA_OOM");
  assert.ok(sidecar.remediation_hints.some((hint) => hint.includes("lowVram")));
  assert.equal(sidecar.metadata.execution_mode, "persistent_server");
  assert.ok(sidecar.quality.warning_codes.includes("PLACEHOLDER_PROMPT"));
});

test("storage roots are explicit", () => {
  const roots = resolveStorageRoots({
    cwd: "C:/work/project",
    outputRoot: "assets",
    tempRoot: "tmp",
    cacheRoot: "cache"
  });

  assert.equal(roots.outputRoot, path.resolve("C:/work/project", "assets"));
  assert.equal(roots.tempRoot, path.resolve("C:/work/project", "tmp"));
  assert.equal(roots.cacheRoot, path.resolve("C:/work/project", "cache"));
});

test("cleanupStorage keeps output images and removes sidecars logs and temp files", async () => {
  const tempDir = await mkdtemp(path.join(os.tmpdir(), "tcxsd-cleanup-"));
  const roots = {
    outputRoot: path.join(tempDir, "out"),
    tempRoot: path.join(tempDir, "tmp"),
    cacheRoot: path.join(tempDir, "cache")
  };
  await mkdir(roots.outputRoot, { recursive: true });
  await mkdir(roots.tempRoot, { recursive: true });
  await writeFile(path.join(roots.outputRoot, "keep.png"), "image");
  await writeFile(path.join(roots.outputRoot, "old.json"), "{}");
  await writeFile(path.join(roots.outputRoot, "old.log"), "log");
  await writeFile(path.join(roots.tempRoot, "old.png"), "tmp");

  try {
    const removed = await cleanupStorage({ roots, olderThanMs: 0, dryRun: false });

    assert.ok(existsSync(path.join(roots.outputRoot, "keep.png")));
    assert.deepEqual(removed.map((item) => path.basename(item)).sort(), ["old.json", "old.log", "old.png"]);
  } finally {
    await rm(tempDir, { recursive: true, force: true });
  }
});

test("prompt pack preserves Chinese text", () => {
  const packed = promptPacks.ideogram4Poster({
    subject: "一张展示本地 AI 生图工作流的中文海报",
    visibleText: "本地生图",
    language: "zh"
  });
  const encoded = JSON.stringify(packed);

  assert.match(packed.prompt_json.high_level_description, /本地生图/);
  assert.match(encoded, /中文海报/);
});

test("cancel image job posts to the server cancel endpoint", async () => {
  const calls = [];
  const originalFetch = globalThis.fetch;
  globalThis.fetch = async (url, options) => {
    calls.push([url, options?.method]);
    return {
      ok: true,
      status: 200,
      async json() {
        return { ok: true };
      },
      async text() {
        return "{}";
      }
    };
  };
  try {
    await cancelImageJob({ id: "job-1", poll_url: "/sdcpp/v1/jobs/job-1" }, { host: "127.0.0.1", port: 19999 });
  } finally {
    globalThis.fetch = originalFetch;
  }

  assert.deepEqual(calls, [["http://127.0.0.1:19999/sdcpp/v1/jobs/job-1/cancel", "POST"]]);
});

test("typed errors carry code and remediation hints", () => {
  const error = new TcxSdError("CUDA out of memory", { cause: "unit-test" });

  assert.equal(error.code, "CUDA_OOM");
  assert.ok(error.remediationHints.some((hint) => hint.includes("lowVram")));
});

test("quality assessment reports placeholder prompts", () => {
  const report = assessResultQuality({
    ok: true,
    metadata: {
      prompt: "TODO {subject}",
      width: "512",
      height: "512"
    },
    image_width: 512,
    image_height: 512
  });

  assert.equal(report.ok, true);
  assert.ok(report.warning_codes.includes("PLACEHOLDER_PROMPT"));
});

test("runTextToImage writes a failure sidecar for submit errors", async () => {
  const tempDir = await mkdtemp(path.join(os.tmpdir(), "tcxsd-node-"));
  const sidecarPath = path.join(tempDir, "failed.json");
  const originalFetch = globalThis.fetch;
  globalThis.fetch = async () => ({
    ok: false,
    status: 500,
    async text() {
      return "CUDA out of memory";
    }
  });
  try {
    await assert.rejects(
      runTextToImage({
        manageServer: false,
        sidecar: sidecarPath,
        output: path.join(tempDir, "failed.png"),
        prompt: "failure case"
      }),
      (error) => error.code === "CUDA_OOM"
    );
    const sidecar = JSON.parse(await readFile(sidecarPath, "utf8"));
    assert.equal(sidecar.ok, false);
    assert.equal(sidecar.error_code, "CUDA_OOM");
    assert.match(sidecar.error, /CUDA out of memory/);
  } finally {
    globalThis.fetch = originalFetch;
    await rm(tempDir, { recursive: true, force: true });
  }
});
