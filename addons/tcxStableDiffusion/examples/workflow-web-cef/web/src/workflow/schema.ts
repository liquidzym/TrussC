export type WorkflowNodeKind =
  | "ModelProfile"
  | "RuntimePreset"
  | "Prompt"
  | "NegativePrompt"
  | "SourceImage"
  | "MaskImage"
  | "ControlNet"
  | "LoRAStack"
  | "Generate"
  | "QualityCheck"
  | "SaveArtifact"
  | "BatchSeeds";

export type WorkflowEdgeKind =
  | "prompt"
  | "negativePrompt"
  | "image"
  | "mask"
  | "control"
  | "lora"
  | "settings"
  | "artifact";

export interface WorkflowNode<T = Record<string, unknown>> {
  id: string;
  kind: WorkflowNodeKind;
  label: string;
  x: number;
  y: number;
  data: T;
}

export interface WorkflowEdge {
  id: string;
  from: string;
  to: string;
  kind: WorkflowEdgeKind;
}

export interface TxcSdWorkflow {
  schema: "tcxsd.workflow.v1";
  id: string;
  title: string;
  language: "en" | "zh";
  nodes: WorkflowNode[];
  edges: WorkflowEdge[];
}

export type WorkflowCommandType =
  | "validateWorkflow"
  | "runWorkflow"
  | "cancelJob"
  | "listModels"
  | "listLoras"
  | "openSidecar";

export interface BridgeCommand {
  id: string;
  type: WorkflowCommandType;
  workflow?: TxcSdWorkflow;
  jobId?: string;
  payload?: Record<string, unknown>;
}

export interface BackendError {
  code: string;
  message: string;
  remediation_hints?: string[];
}

export interface BackendMessage {
  id?: string;
  type: string;
  ok?: boolean;
  stage?: string;
  detail?: string;
  jobId?: string;
  error?: BackendError;
  result?: Record<string, unknown>;
  models?: string[];
  loras?: Array<Record<string, unknown>>;
  validation?: unknown;
}
