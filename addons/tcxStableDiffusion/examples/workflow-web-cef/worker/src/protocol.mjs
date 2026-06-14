export const workerErrorHints = {
  WORKFLOW_INVALID: [
    "检查工作流图，并确保只有一个生成节点。",
    "运行前连接必要的提示词、源图、遮罩图、ControlNet 和 LoRA 节点。"
  ],
  MODEL_ASSET_MISSING: [
    "确认引用文件位于 bin/data 下。",
    "打包前先运行资产准备工具。"
  ],
  BACKEND_UNSUPPORTED: [
    "这个工作流请使用 persistent sd-server 后端。",
    "移除当前后端不支持的图像、遮罩、ControlNet 或 LoRA 字段。"
  ],
  CANCEL_NOT_INTERRUPTIBLE: [
    "取消请求已发送，但当前后端步骤可能会先完成再停止。",
    "如果需要更快中断，请先用草稿质量重试，或重启应用。"
  ],
  WORKER_UNAVAILABLE: [
    "重启应用。",
    "检查内置 Node runtime 和 worker dist 文件。"
  ],
  UNKNOWN: [
    "打开 bin/data/workflows/logs 下的侧车 JSON 或后端日志。",
    "先用草稿质量重试，以区分模型问题和运行时问题。"
  ]
};

export class WorkflowWorkerError extends Error {
  constructor(message, { code = "UNKNOWN", remediationHints = undefined, details = undefined } = {}) {
    super(message);
    this.name = "WorkflowWorkerError";
    this.code = code;
    this.remediationHints = remediationHints || workerErrorHints[code] || workerErrorHints.UNKNOWN;
    this.details = details;
  }
}

export function ok(id, type, payload = {}) {
  return { id, type, ok: true, ...payload };
}

export function fail(id, error) {
  const code = error?.code || "UNKNOWN";
  return {
    id,
    type: "error",
    ok: false,
    error: {
      code,
      message: error?.message || String(error),
      remediation_hints: error?.remediationHints || error?.remediation_hints || workerErrorHints[code] || workerErrorHints.UNKNOWN
    }
  };
}

export function progress(id, jobId, stage, detail = "") {
  return { id, type: "progress", ok: true, jobId, stage, detail };
}
