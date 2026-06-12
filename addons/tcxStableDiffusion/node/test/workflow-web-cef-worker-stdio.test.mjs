import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import test from "node:test";
import path from "node:path";

test("worker stdio errors preserve the frontend command id", async () => {
  const addonRoot = path.resolve(import.meta.dirname, "..", "..");
  const workerScript = path.join(addonRoot, "examples", "workflow-web-cef", "worker", "src", "worker.mjs");
  const cwd = path.join(addonRoot, "examples", "workflow-web-cef", "bin");
  const workflow = {
    schema: "tcxsd.workflow.v1",
    id: "lora-stack",
    title: "LoRA stack smoke",
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
  };

  const messages = await runWorker(workerScript, cwd, { id: "lora-error", type: "runWorkflow", workflow });
  const error = messages.find((message) => message.type === "error");

  assert.equal(error?.id, "lora-error");
  assert.equal(error?.error?.code, "MODEL_ASSET_MISSING");
});

function runWorker(workerScript, cwd, command) {
  return new Promise((resolve, reject) => {
    const child = spawn(process.execPath, [workerScript], { cwd, stdio: ["pipe", "pipe", "pipe"] });
    const messages = [];
    let stdout = "";
    let stderr = "";
    const timer = setTimeout(() => {
      child.kill();
      reject(new Error(`worker timed out: ${stderr}`));
    }, 15000);

    child.stdout.on("data", (chunk) => {
      stdout += chunk.toString("utf8");
      let newline;
      while ((newline = stdout.indexOf("\n")) >= 0) {
        const line = stdout.slice(0, newline).trim();
        stdout = stdout.slice(newline + 1);
        if (!line) continue;
        const message = JSON.parse(line);
        messages.push(message);
        if (message.type === "workerReady") {
          child.stdin.write(`${JSON.stringify(command)}\n`);
        }
        if (message.type === "error") {
          child.stdin.write(`${JSON.stringify({ id: "shutdown", type: "shutdown" })}\n`);
        }
      }
    });
    child.stderr.on("data", (chunk) => {
      stderr += chunk.toString("utf8");
    });
    child.on("error", reject);
    child.on("exit", () => {
      clearTimeout(timer);
      resolve(messages);
    });
  });
}
