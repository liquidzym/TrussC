import assert from "node:assert/strict";
import path from "node:path";
import test from "node:test";
import {
  buildImageRequest,
  buildServerArgs,
  defaultExampleRoot,
  defaultModelRoot,
  extractImageBase64,
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
