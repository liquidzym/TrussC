import { existsSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

import { WorkflowWorkerError } from "./protocol.mjs";

const moduleDir = path.dirname(fileURLToPath(import.meta.url));
let sdkPromise = null;

export function validateWorkflowForBackend(workflow) {
  const errors = [];
  const warnings = [];
  const nodes = Array.isArray(workflow?.nodes) ? workflow.nodes : [];
  const edges = Array.isArray(workflow?.edges) ? workflow.edges : [];
  const ids = new Set();

  if (workflow?.schema !== "tcxsd.workflow.v1") {
    errors.push({ code: "WORKFLOW_INVALID", message: "工作流 schema 必须是 tcxsd.workflow.v1。" });
  }

  for (const node of nodes) {
    if (!node?.id) {
      errors.push({ code: "WORKFLOW_INVALID", message: "每个节点都需要 ID。" });
      continue;
    }
    if (ids.has(node.id)) {
      errors.push({ code: "DUPLICATE_NODE_ID", message: `节点 ID 重复：${node.id}`, nodeId: node.id });
    }
    ids.add(node.id);
  }

  const generateNodes = nodesOf(workflow, "Generate");
  if (generateNodes.length !== 1) {
    errors.push({ code: "WORKFLOW_INVALID", message: "工作流必须且只能有一个生成节点。" });
  }

  for (const edge of edges) {
    if (!ids.has(edge?.from) || !ids.has(edge?.to)) {
      errors.push({ code: "MISSING_EDGE_ENDPOINT", message: `连线缺少端点：${edge?.id || ""}` });
    }
  }

  for (const node of nodesOf(workflow, "ControlNet")) {
    if (!stringField(node, "controlImage")) {
      errors.push({ code: "WORKFLOW_INVALID", message: "ControlNet 需要明确的控制图路径。", nodeId: node.id });
    }
  }

  const generateMode = String(generateNodes[0]?.data?.requestMode || "");
  if (generateMode === "inpaint" || nodesOf(workflow, "MaskImage").length > 0) {
    if (nodesOf(workflow, "SourceImage").length === 0 || nodesOf(workflow, "MaskImage").length === 0) {
      errors.push({ code: "WORKFLOW_INVALID", message: "局部重绘需要源图和遮罩图节点。" });
    }
  }

  for (const node of nodesOf(workflow, "LoRAStack")) {
    const loras = Array.isArray(node?.data?.loras) ? node.data.loras : [];
    if (!loras.length) {
      errors.push({ code: "MODEL_ASSET_MISSING", message: "LoRA 栈至少需要一个 LoRA。", nodeId: node.id });
    }
  }

  if (nodesOf(workflow, "QualityCheck").length === 0) {
    warnings.push({ code: "QUALITY_CHECK_MISSING", message: "建议在最终保存前加入质量检查节点。" });
  }

  return { ok: errors.length === 0, errors, warnings };
}

export async function buildRequestFromWorkflow(workflow, context = {}) {
  const validation = validateWorkflowForBackend(workflow);
  if (!validation.ok) {
    throw new WorkflowWorkerError(validation.errors.map((item) => item.message).join("; "), {
      code: validation.errors[0]?.code || "WORKFLOW_INVALID",
      details: { validation }
    });
  }

  const sdk = await loadSdk();
  const cwd = path.resolve(context.cwd || process.cwd());
  const dataRoot = path.resolve(cwd, context.dataRoot || defaultDataRoot(cwd));
  const workflowRoot = path.join(dataRoot, "workflows");
  const outputRoot = path.resolve(context.outputRoot || path.join(workflowRoot, "outputs"));
  const tempRoot = path.resolve(context.tempRoot || path.join(workflowRoot, "tmp"));
  const cacheRoot = path.resolve(context.cacheRoot || path.join(workflowRoot, "cache"));
  const logRoot = path.resolve(context.logRoot || path.join(workflowRoot, "logs"));
  const modelRoot = path.resolve(cwd, context.modelRoot || path.join(dataRoot, "models"));
  const loraModelDir = path.resolve(cwd, context.loraModelDir || path.join(modelRoot, "loras"));
  const serverExecutable = path.resolve(cwd, context.serverExecutable || executableName("sd-server"));

  const generateNode = one(workflow, "Generate");
  const modelNode = one(workflow, "ModelProfile");
  const runtimeNode = one(workflow, "RuntimePreset");
  const promptNode = one(workflow, "Prompt");
  const negativeNode = one(workflow, "NegativePrompt");
  const sourceNode = one(workflow, "SourceImage");
  const maskNode = one(workflow, "MaskImage");
  const controlNode = one(workflow, "ControlNet");
  const loraNode = one(workflow, "LoRAStack");
  const batchNode = one(workflow, "BatchSeeds");

  const mode = String(generateNode?.data?.requestMode || (controlNode ? "control_net" : maskNode ? "inpaint" : sourceNode ? "image_to_image" : "text_to_image"));
  const outputName = sanitizeName(generateNode?.data?.outputName || workflow.id || "workflow");
  const common = {
    model: String(modelNode?.data?.model || context.model || "flux2-klein-4b-q4_0"),
    modelRoot,
    serverExecutable,
    outputRoot,
    tempRoot,
    cacheRoot,
    logRoot,
    loraModelDir,
    quality: String(runtimeNode?.data?.quality || generateNode?.data?.quality || "draft"),
    runtimePreset: String(runtimeNode?.data?.runtimePreset || generateNode?.data?.runtimePreset || "lowVram"),
    width: numberField(runtimeNode, "width") || numberField(generateNode, "width") || undefined,
    height: numberField(runtimeNode, "height") || numberField(generateNode, "height") || undefined,
    steps: numberField(runtimeNode, "steps") || numberField(generateNode, "steps") || undefined,
    cfgScale: numberField(runtimeNode, "cfgScale") || numberField(generateNode, "cfgScale") || undefined,
    seed: numberOrUndefined(generateNode?.data?.seed),
    prompt: String(promptNode?.data?.prompt || ""),
    negativePrompt: String(negativeNode?.data?.negativePrompt || ""),
    output: path.join(outputRoot, `${outputName}.png`),
    sidecar: path.join(outputRoot, `${outputName}.json`),
    metadata: {
      workflow_id: String(workflow.id || ""),
      workflow_title: String(workflow.title || ""),
      language: String(workflow.language || "zh-CN"),
      request_mode: mode
    },
    executionMode: context.executionMode || "persistent_server",
    timeoutMs: numberOrUndefined(context.timeoutMs) || numberOrUndefined(generateNode?.data?.timeoutMs),
    pollMs: numberOrUndefined(context.pollMs) || undefined
  };

  let request;
  if (mode === "image_to_image") {
    request = sdk.createImageToImageRequest({
      ...common,
      initImage: resolveWorkflowPath(sourceNode?.data?.path, cwd),
      strength: numberField(generateNode, "strength") || undefined
    });
  } else if (mode === "inpaint") {
    request = sdk.createInpaintRequest({
      ...common,
      initImage: resolveWorkflowPath(sourceNode?.data?.path, cwd),
      maskImage: resolveWorkflowPath(maskNode?.data?.path, cwd),
      strength: numberField(generateNode, "strength") || undefined
    });
  } else if (mode === "control_net" || mode === "controlnet") {
    request = sdk.createControlNetRequest({
      ...common,
      requestMode: "control_net",
      controlImage: resolveWorkflowPath(controlNode?.data?.controlImage, cwd),
      controlStrength: numberField(controlNode, "controlStrength") || numberField(generateNode, "controlStrength") || 1.0
    });
  } else if (mode === "lora_stack") {
    const loras = normalizeLoras(loraNode?.data?.loras || [], loraModelDir);
    request = sdk.createLoraStackRequest({
      ...common,
      requestMode: "lora_stack",
      loras
    });
  } else {
    request = sdk.createTextToImageRequest(common);
  }

  sdk.assertRequestSupported(request, {
    executionMode: common.executionMode,
    model: common.model,
    hasControlNet: Boolean(request.controlImage),
    loraModelDir: request.loras?.length ? loraModelDir : undefined
  });

  assertReferencedAssetsExist(request);

  const seeds = Array.isArray(batchNode?.data?.seeds) ? batchNode.data.seeds.map(Number).filter(Number.isFinite) : [];
  if (seeds.length) request.batchSeeds = seeds;
  return request;
}

export async function listAvailableModels() {
  const sdk = await loadSdk();
  return Object.keys(sdk.modelProfiles || {});
}

export async function listAvailableLoras(context = {}) {
  const sdk = await loadSdk();
  const cwd = path.resolve(context.cwd || process.cwd());
  const dataRoot = path.resolve(cwd, context.dataRoot || defaultDataRoot(cwd));
  const modelRoot = path.resolve(cwd, context.modelRoot || path.join(dataRoot, "models"));
  const loraModelDir = path.resolve(cwd, context.loraModelDir || path.join(modelRoot, "loras"));
  return sdk.listLoras({ loraModelDir });
}

export async function loadSdk() {
  if (!sdkPromise) {
    sdkPromise = importSdk();
  }
  return sdkPromise;
}

async function importSdk() {
  const candidates = [
    path.resolve(moduleDir, "../../../../node/src/index.mjs"),
    path.resolve(moduleDir, "../../../runtime/node-package/src/index.mjs")
  ];
  for (const candidate of candidates) {
    if (existsSync(candidate)) {
      return import(pathToFileUrl(candidate));
    }
  }
  return import("@trussc/tcx-stable-diffusion");
}

function nodesOf(workflow, kind) {
  return (workflow?.nodes || []).filter((node) => node?.kind === kind);
}

function one(workflow, kind) {
  return nodesOf(workflow, kind)[0];
}

function stringField(node, key) {
  const value = node?.data?.[key];
  return typeof value === "string" ? value.trim() : "";
}

function numberField(node, key) {
  return numberOrUndefined(node?.data?.[key]);
}

function numberOrUndefined(value) {
  const number = Number(value);
  return Number.isFinite(number) ? number : undefined;
}

function defaultDataRoot(cwd) {
  return path.basename(cwd).toLowerCase() === "bin" ? "data" : "bin/data";
}

function executableName(base) {
  return process.platform === "win32" ? `${base}.exe` : base;
}

function resolveWorkflowPath(value, cwd) {
  if (!value) return undefined;
  const text = String(value).replace(/\\/g, "/");
  if (path.isAbsolute(text)) return path.normalize(text);
  if (path.basename(cwd).toLowerCase() === "bin" && text.startsWith("bin/")) {
    return path.resolve(cwd, text.slice(4));
  }
  return path.resolve(cwd, text);
}

function normalizeLoras(loras, loraModelDir) {
  return loras.map((item) => {
    const loraPath = String(item?.path || "");
    const absolute = path.isAbsolute(loraPath) ? loraPath : path.join(loraModelDir, loraPath);
    if (!existsSync(absolute)) {
      throw new WorkflowWorkerError(`LoRA 资源缺失：${loraPath}`, {
        code: "MODEL_ASSET_MISSING",
        remediationHints: [
          "把 .safetensors LoRA 放到 bin/data/models/loras 下。",
          "在工作流界面点击 LoRA 列表，并选择扫描到的相对路径。"
        ],
        details: { loraModelDir, path: loraPath }
      });
    }
    return {
      path: loraPath,
      weight: Number(item?.weight ?? item?.multiplier ?? 1)
    };
  });
}

function assertReferencedAssetsExist(request) {
  for (const [key, value] of Object.entries({
    initImage: request.initImage,
    maskImage: request.maskImage,
    controlImage: request.controlImage
  })) {
    if (value && !existsSync(value)) {
      throw new WorkflowWorkerError(`${assetLabel(key)} 资源缺失：${value}`, {
        code: "MODEL_ASSET_MISSING",
        remediationHints: [
          "确认引用文件位于 bin/data 下。",
          "打包前先运行资产准备工具。"
        ],
        details: { key, path: value }
      });
    }
  }
}

function assetLabel(key) {
  if (key === "initImage") return "源图";
  if (key === "maskImage") return "遮罩图";
  if (key === "controlImage") return "控制图";
  return key;
}

function sanitizeName(value) {
  return String(value || "workflow").replace(/[^a-z0-9_.-]+/gi, "_").replace(/^_+|_+$/g, "") || "workflow";
}

function pathToFileUrl(value) {
  const resolved = path.resolve(value).replace(/\\/g, "/");
  return `file:///${resolved.replace(/^\/+/, "")}`;
}
