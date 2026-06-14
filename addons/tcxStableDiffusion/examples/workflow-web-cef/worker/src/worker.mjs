import { createInterface } from "node:readline";
import { readFile } from "node:fs/promises";
import path from "node:path";

import { fail, ok, progress, WorkflowWorkerError } from "./protocol.mjs";
import {
  buildRequestFromWorkflow,
  listAvailableLoras,
  listAvailableModels,
  loadSdk,
  validateWorkflowForBackend
} from "./workflow-executor.mjs";

const context = {
  cwd: process.cwd()
};
const sessions = new Map();
const activeJobs = new Map();

process.on("SIGINT", () => shutdown(130));
process.on("SIGTERM", () => shutdown(143));
process.on("exit", () => closeSessions());

const input = createInterface({ input: process.stdin, crlfDelay: Infinity });
input.on("line", async (line) => {
  const text = line.trim();
  if (!text) return;
  let command = null;
  try {
    command = JSON.parse(text);
  } catch (error) {
    emit(fail("", error));
    return;
  }
  try {
    await handleCommand(command);
  } catch (error) {
    emit(fail(command?.id || "", error));
  }
});

emit(ok("worker-ready", "workerReady", { detail: "工作流 Worker 已就绪" }));

async function handleCommand(command) {
  if (command.type === "validateWorkflow") {
    const validation = validateWorkflowForBackend(command.workflow);
    emit(ok(command.id, "validation", { validation, detail: validation.ok ? "工作流有效" : "工作流无效" }));
    return;
  }

  if (command.type === "listModels") {
    emit(ok(command.id, "listModels", { models: await listAvailableModels() }));
    return;
  }

  if (command.type === "listLoras") {
    emit(ok(command.id, "listLoras", { loras: await listAvailableLoras(context) }));
    return;
  }

  if (command.type === "openSidecar") {
    emit(ok(command.id, "sidecar", { result: { text: await readSidecar(command.payload?.path) } }));
    return;
  }

  if (command.type === "cancelJob") {
    cancelJob(command);
    return;
  }

  if (command.type === "runWorkflow") {
    await runWorkflow(command);
    return;
  }

  if (command.type === "shutdown") {
    emit(ok(command.id, "shutdown", { detail: "已请求关闭 Worker" }));
    shutdown(0);
    return;
  }

  throw new WorkflowWorkerError(`未知 Worker 命令：${command.type}`, { code: "WORKFLOW_INVALID" });
}

async function runWorkflow(command) {
  const request = await buildRequestFromWorkflow(command.workflow, context);
  const sdk = await loadSdk();
  const jobId = `${command.workflow?.id || "workflow"}-${Date.now()}`;
  const abort = new AbortController();
  activeJobs.set(jobId, abort);

  emit(progress(command.id, jobId, "queued", "工作流已进入队列"));
  try {
    const session = await sessionForRequest(sdk, request);
    emit(progress(command.id, jobId, "running", `${request.model} ${request.requestMode || "text_to_image"}`));
    const result = request.batchSeeds?.length
      ? await runBatchSeeds(sdk, session, request, abort.signal)
      : await session.generate({ ...request, signal: abort.signal });
    const first = Array.isArray(result) ? result[0] : result;
    emit(ok(command.id, "result", {
      jobId,
      result: {
        ...first,
        workflowId: command.workflow?.id || "",
        seed: request.seed
      },
      detail: "生成完成"
    }));
  } catch (error) {
    emit(fail(command.id, error));
  } finally {
    activeJobs.delete(jobId);
  }
}

async function runBatchSeeds(sdk, session, request, signal) {
  const batch = sdk.createBatchJob({ label: request.metadata?.workflow_id || "workflow", baseRequest: request });
  batch.seedSweep(request.batchSeeds, request);
  return session.runBatch(batch, { ...request, signal });
}

async function sessionForRequest(sdk, request) {
  const key = `${request.model}|${request.runtimePreset}`;
  const existing = sessions.get(key);
  if (existing) return existing;
  const session = sdk.createGenerationSession({
    ...request,
    id: key,
    reuseServer: false,
    executionMode: request.executionMode || "persistent_server"
  });
  await session.start();
  sessions.set(key, session);
  return session;
}

function cancelJob(command) {
  const job = activeJobs.get(command.jobId);
  if (!job) {
    throw new WorkflowWorkerError(`没有可取消的活动任务：${command.jobId}`, {
      code: "CANCEL_NOT_INTERRUPTIBLE"
    });
  }
  job.abort();
  emit(ok(command.id, "cancelled", { jobId: command.jobId, detail: "已请求取消任务" }));
}

async function readSidecar(value) {
  const text = String(value || "");
  if (!text) {
    throw new WorkflowWorkerError("侧车路径为空。", { code: "WORKFLOW_INVALID" });
  }
  const absolute = path.resolve(context.cwd, text);
  return readFile(absolute, "utf8");
}

function closeSessions() {
  for (const session of sessions.values()) {
    session.close();
  }
  sessions.clear();
}

function shutdown(code) {
  closeSessions();
  process.exit(code);
}

function emit(message) {
  process.stdout.write(`${JSON.stringify(message)}\n`);
}
