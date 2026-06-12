import type { TxcSdWorkflow, WorkflowNode } from "./schema";

export interface WorkflowValidation {
  ok: boolean;
  errors: Array<{ code: string; message: string; nodeId?: string }>;
  warnings: Array<{ code: string; message: string; nodeId?: string }>;
}

function nodesOf(workflow: TxcSdWorkflow, kind: WorkflowNode["kind"]) {
  return workflow.nodes.filter((node) => node.kind === kind);
}

function hasEdgeTo(workflow: TxcSdWorkflow, nodeId: string, kind: string) {
  return workflow.edges.some((edge) => edge.to === nodeId && edge.kind === kind);
}

export function validateWorkflow(workflow: TxcSdWorkflow): WorkflowValidation {
  const errors: WorkflowValidation["errors"] = [];
  const warnings: WorkflowValidation["warnings"] = [];
  const ids = new Set<string>();

  for (const node of workflow.nodes) {
    if (ids.has(node.id)) {
      errors.push({ code: "DUPLICATE_NODE_ID", message: `节点 ID 重复：${node.id}`, nodeId: node.id });
    }
    ids.add(node.id);
  }

  for (const edge of workflow.edges) {
    if (!ids.has(edge.from) || !ids.has(edge.to)) {
      errors.push({ code: "MISSING_EDGE_ENDPOINT", message: `连线缺少端点：${edge.id}` });
    }
  }

  const generateNodes = nodesOf(workflow, "Generate");
  if (generateNodes.length !== 1) {
    errors.push({ code: "WORKFLOW_INVALID", message: "工作流必须且只能有一个生成节点。" });
  }

  for (const node of nodesOf(workflow, "ControlNet")) {
    const image = String(node.data.controlImage || "");
    if (!image && !hasEdgeTo(workflow, node.id, "control")) {
      errors.push({ code: "WORKFLOW_INVALID", message: "ControlNet 需要控制图或控制图连线。", nodeId: node.id });
    }
  }

  const generateMode = String(generateNodes[0]?.data.requestMode || "");
  if (generateMode === "inpaint" || nodesOf(workflow, "MaskImage").length > 0) {
    if (nodesOf(workflow, "SourceImage").length === 0 || nodesOf(workflow, "MaskImage").length === 0) {
      errors.push({ code: "WORKFLOW_INVALID", message: "局部重绘需要源图和遮罩图节点。" });
    }
  }

  for (const node of nodesOf(workflow, "LoRAStack")) {
    const loras = Array.isArray(node.data.loras) ? node.data.loras : [];
    if (loras.length === 0) {
      errors.push({ code: "MODEL_ASSET_MISSING", message: "LoRA 栈至少需要一个 LoRA。", nodeId: node.id });
    }
  }

  if (nodesOf(workflow, "QualityCheck").length === 0) {
    warnings.push({ code: "QUALITY_CHECK_MISSING", message: "建议在最终保存前加入质量检查节点。" });
  }

  return { ok: errors.length === 0, errors, warnings };
}
