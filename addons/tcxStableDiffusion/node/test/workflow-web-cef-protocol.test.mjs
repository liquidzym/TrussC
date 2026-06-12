import assert from "node:assert/strict";
import test from "node:test";
import { mkdtemp, rm } from "node:fs/promises";
import os from "node:os";
import path from "node:path";

import {
  buildRequestFromWorkflow,
  validateWorkflowForBackend
} from "../../examples/workflow-web-cef/worker/src/workflow-executor.mjs";

function baseWorkflow(overrides = {}) {
  return {
    schema: "tcxsd.workflow.v1",
    id: "text-to-image.zh",
    title: "Chinese poster smoke",
    language: "zh",
    nodes: [
      { id: "model", kind: "ModelProfile", label: "Model", x: 0, y: 0, data: { model: "ideogram4-q4_0" } },
      { id: "runtime", kind: "RuntimePreset", label: "Runtime", x: 0, y: 0, data: { runtimePreset: "lowVram", quality: "draft", width: 512, height: 512, steps: 8 } },
      { id: "prompt", kind: "Prompt", label: "Prompt", x: 0, y: 0, data: { prompt: "本地生成，清晰中文海报" } },
      { id: "generate", kind: "Generate", label: "Generate", x: 0, y: 0, data: { requestMode: "text_to_image", outputName: "zh_smoke" } }
    ],
    edges: [
      { id: "model-generate", from: "model", to: "generate", kind: "settings" },
      { id: "runtime-generate", from: "runtime", to: "generate", kind: "settings" },
      { id: "prompt-generate", from: "prompt", to: "generate", kind: "prompt" }
    ],
    ...overrides
  };
}

test("valid Chinese workflow keeps visible Chinese text in the generated request", async () => {
  const workflow = baseWorkflow();
  const request = await buildRequestFromWorkflow(workflow, {
    cwd: process.cwd(),
    dataRoot: "bin/data",
    modelRoot: "bin/data/models"
  });

  assert.match(request.prompt, /本地生成/);
  assert.equal(request.metadata.workflow_id, "text-to-image.zh");
  assert.equal(request.metadata.language, "zh");
});

test("missing Generate node returns WORKFLOW_INVALID", () => {
  const workflow = baseWorkflow({ nodes: baseWorkflow().nodes.filter((node) => node.kind !== "Generate") });
  const validation = validateWorkflowForBackend(workflow);

  assert.equal(validation.ok, false);
  assert.equal(validation.errors[0].code, "WORKFLOW_INVALID");
});

test("ControlNet without a control image returns WORKFLOW_INVALID", () => {
  const workflow = baseWorkflow({
    id: "controlnet-canny",
    language: "en",
    nodes: [
      { id: "model", kind: "ModelProfile", label: "Model", x: 0, y: 0, data: { model: "sd15-controlnet-canny" } },
      { id: "control", kind: "ControlNet", label: "Control", x: 0, y: 0, data: { controlStrength: 1.0 } },
      { id: "prompt", kind: "Prompt", label: "Prompt", x: 0, y: 0, data: { prompt: "guided generation" } },
      { id: "generate", kind: "Generate", label: "Generate", x: 0, y: 0, data: { requestMode: "control_net" } }
    ],
    edges: [
      { id: "control-generate", from: "control", to: "generate", kind: "control" },
      { id: "prompt-generate", from: "prompt", to: "generate", kind: "prompt" }
    ]
  });
  const validation = validateWorkflowForBackend(workflow);

  assert.equal(validation.ok, false);
  assert.equal(validation.errors[0].code, "WORKFLOW_INVALID");
});

test("missing LoRA returns MODEL_ASSET_MISSING", async () => {
  const temp = await mkdtemp(path.join(os.tmpdir(), "tcxsd-lora-"));
  try {
    const workflow = baseWorkflow({
      id: "lora-stack",
      language: "en",
      nodes: [
        { id: "model", kind: "ModelProfile", label: "Model", x: 0, y: 0, data: { model: "flux2-klein-4b-q4_0" } },
        { id: "prompt", kind: "Prompt", label: "Prompt", x: 0, y: 0, data: { prompt: "styled image" } },
        { id: "lora", kind: "LoRAStack", label: "LoRA", x: 0, y: 0, data: { loras: [{ path: "starter/style.safetensors", weight: 0.65 }] } },
        { id: "generate", kind: "Generate", label: "Generate", x: 0, y: 0, data: { requestMode: "lora_stack" } }
      ],
      edges: [
        { id: "prompt-generate", from: "prompt", to: "generate", kind: "prompt" },
        { id: "lora-generate", from: "lora", to: "generate", kind: "lora" }
      ]
    });

    await assert.rejects(
      () => buildRequestFromWorkflow(workflow, {
        cwd: process.cwd(),
        dataRoot: temp,
        modelRoot: path.join(temp, "models")
      }),
      (error) => error.code === "MODEL_ASSET_MISSING"
    );
  } finally {
    await rm(temp, { force: true, recursive: true });
  }
});

test("unsupported backend returns BACKEND_UNSUPPORTED with remediation hints", async () => {
  const workflow = baseWorkflow({
    id: "controlnet-canny",
    language: "en",
    nodes: [
      { id: "model", kind: "ModelProfile", label: "Model", x: 0, y: 0, data: { model: "sd15-controlnet-canny" } },
      { id: "control", kind: "ControlNet", label: "Control", x: 0, y: 0, data: { controlImage: "bin/data/inputs/control/canny-guide.png", controlStrength: 1.0 } },
      { id: "prompt", kind: "Prompt", label: "Prompt", x: 0, y: 0, data: { prompt: "guided generation" } },
      { id: "generate", kind: "Generate", label: "Generate", x: 0, y: 0, data: { requestMode: "control_net" } }
    ],
    edges: [
      { id: "control-generate", from: "control", to: "generate", kind: "control" },
      { id: "prompt-generate", from: "prompt", to: "generate", kind: "prompt" }
    ]
  });

  await assert.rejects(
    () => buildRequestFromWorkflow(workflow, {
      cwd: process.cwd(),
      executionMode: "in_process",
      dataRoot: "bin/data",
      modelRoot: "bin/data/models"
    }),
    (error) => error.code === "BACKEND_UNSUPPORTED" && error.remediationHints?.length > 0
  );
});
