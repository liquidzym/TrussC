export type Delegate = "GPU" | "CPU";
export type InputMode = "WebCamera" | "ExternalFrame";
export type TaskName = "hand" | "pose" | "face" | "gesture";

export type RuntimeConfig = {
  type: "config";
  delegate: Delegate;
  inputMode: InputMode;
  tasks: Record<TaskName, boolean>;
  maxFPS: number;
  inputWidth: number;
  inputHeight: number;
  processingWidth: number;
  processingHeight: number;
  mirror: boolean;
  multiPerson: boolean;
  maxHands: number;
  maxPoses: number;
  maxFaces: number;
  maxGestures: number;
  outputFaceBlendshapes: boolean;
  outputFaceTransformationMatrix: boolean;
  keepRunningWhenHidden: boolean;
};

export type RuntimeGpuInfo = {
  webglVendor?: string;
  webglRenderer?: string;
  webglVersion?: string;
  webglShadingLanguageVersion?: string;
};

export type RuntimeStatus = {
  type: "runtime_status";
  ready?: boolean;
  cameraReady?: boolean;
  modelReady?: boolean;
  pipelineReady?: boolean;
  activeDelegate?: Delegate;
  fallback?: boolean;
  reason?: string;
  stage?: string;
  detail?: string;
  wasmPath?: string;
  models?: Record<TaskName, boolean>;
  gpu?: RuntimeGpuInfo;
  processingWidth?: number;
  processingHeight?: number;
};

export type RuntimeStats = {
  sourceFPS: number;
  inferenceFPS: number;
  averageInferenceTimeMs: number;
  frameAgeMs: number;
  capturedAtEpochMs: number;
  sentAtEpochMs: number;
};

export type DetectVideoMessage = {
  type: "detect_video";
  timestampMs: number;
  capturedAtEpochMs: number;
  sourceFPS: number;
  frame: ImageBitmap;
};

export type ConfigureWorkerMessage = {
  type: "config";
  delegate: Delegate;
  maxHands: number;
  maxPoses: number;
  maxFaces: number;
  maxGestures: number;
  outputFaceBlendshapes: boolean;
  outputFaceTransformationMatrix: boolean;
};

export type WorkerInboundMessage = DetectVideoMessage | ConfigureWorkerMessage;

export type DetectResultMessage = {
  type: "hand_result" | "pose_result" | "face_result" | "gesture_result";
  timestampMs: number;
  inferenceTimeMs: number;
  stats: RuntimeStats;
  payload: unknown;
};
