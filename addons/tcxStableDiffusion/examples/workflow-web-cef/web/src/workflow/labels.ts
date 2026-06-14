import type { WorkflowNodeKind } from "./schema";

export const nodeKindLabels: Record<WorkflowNodeKind, string> = {
  ModelProfile: "模型档位",
  RuntimePreset: "运行预设",
  Prompt: "提示词",
  NegativePrompt: "负面提示词",
  SourceImage: "源图",
  MaskImage: "遮罩图",
  ControlNet: "ControlNet",
  LoRAStack: "LoRA 栈",
  Generate: "生成",
  QualityCheck: "质量检查",
  SaveArtifact: "保存产物",
  BatchSeeds: "批量种子"
};

const fieldLabels: Record<string, string> = {
  model: "模型",
  runtimePreset: "运行档",
  quality: "质量档",
  width: "宽度",
  height: "高度",
  steps: "步数",
  cfgScale: "CFG",
  prompt: "提示词",
  negativePrompt: "负面提示词",
  requestMode: "生成模式",
  outputName: "输出名",
  rejectBlank: "检测空白图",
  rejectWrongSize: "检测尺寸错误",
  detectTextFailure: "检测文字失败",
  controlImage: "控制图",
  controlStrength: "控制强度",
  path: "路径",
  strength: "重绘强度",
  projectName: "项目名",
  loras: "LoRA 列表",
  seed: "种子",
  seeds: "种子列表"
};

const requestModeLabels: Record<string, string> = {
  text_to_image: "文生图",
  image_to_image: "图生图",
  inpaint: "局部重绘",
  control_net: "ControlNet",
  lora_stack: "LoRA 生成"
};

const statusLabels: Record<string, string> = {
  pending: "等待中",
  progress: "进行中",
  result: "完成",
  error: "失败",
  validation: "检查结果",
  "validation-ok": "检查通过",
  "validation-error": "检查失败",
  "bridge-offline": "桥接未连接",
  workerReady: "Worker 就绪",
  hostStatus: "宿主状态",
  listLoras: "LoRA 列表",
  sidecar: "侧车",
  cancelled: "已请求取消",
  shutdown: "已关闭"
};

const detailLabels: Record<string, string> = {
  "Workflow validation passed": "工作流检查通过",
  "Workflow valid": "工作流有效",
  "Workflow invalid": "工作流无效",
  "Workflow accepted": "工作流已进入队列",
  "Generation complete": "生成完成",
  "Native bridge is not connected": "本地桥接尚未连接",
  "Open through the native host": "请从桌面宿主程序打开",
  "Bridge closed": "桥接已关闭",
  "Bridge error": "桥接连接异常",
  "Workflow worker ready": "工作流 Worker 已就绪",
  "Cancel requested": "已请求取消任务"
};

const hintLabels: Record<string, string> = {
  "Place a .safetensors LoRA under bin/data/models/loras.": "把 .safetensors LoRA 放到 bin/data/models/loras 下。",
  "Run listLoras from the workflow UI and select the discovered relative path.": "在工作流界面点击 LoRA 列表，并选择扫描到的相对路径。",
  "Check that the referenced file exists under bin/data.": "确认引用文件位于 bin/data 下。",
  "Run the asset staging tool before packaging this example.": "打包前先运行资产准备工具。"
};

export function nodeKindLabel(kind: WorkflowNodeKind) {
  return nodeKindLabels[kind] || kind;
}

export function fieldLabel(key: string) {
  return fieldLabels[key] || key;
}

export function requestModeLabel(value: unknown) {
  const key = String(value || "");
  return requestModeLabels[key] || key;
}

export function statusLabel(value: unknown) {
  const key = String(value || "");
  return statusLabels[key] || key;
}

export function detailLabel(value: unknown) {
  const text = String(value || "");
  return detailLabels[text] || text;
}

export function hintLabel(value: unknown) {
  const text = String(value || "");
  return hintLabels[text] || text;
}

export function errorMessageLabel(code: unknown, message: unknown) {
  const raw = String(message || "");
  const value = String(code || "");
  if (value === "MODEL_ASSET_MISSING") {
    const lora = raw.match(/(?:LoRA asset is missing:|LoRA 资源缺失：)\s*(.+)$/);
    if (lora) return `LoRA 资源缺失：${lora[1]}`;
    return "模型或资源文件缺失。";
  }
  if (value === "BACKEND_UNSUPPORTED") return "当前后端不支持这个节点组合。";
  if (value === "WORKFLOW_INVALID") return "工作流结构无效，请检查节点连接。";
  if (value === "CANCEL_NOT_INTERRUPTIBLE") return "已请求取消，但当前生成步骤可能不会立刻中断。";
  if (value === "SERVER_START_FAILED") return "sd-server 启动失败。";
  if (value === "CUDA_OOM") return "CUDA 显存不足。";
  return detailLabel(raw);
}
