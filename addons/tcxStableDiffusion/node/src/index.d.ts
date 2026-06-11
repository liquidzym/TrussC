/// <reference types="node" />

import type { ChildProcess } from "node:child_process";

export const addonRoot: string;
export const defaultExampleRoot: string;
export const defaultNativeDir: string;
export const defaultModelRoot: string;
export const defaultLoraModelDir: string;

export interface QualityDefaults {
  width: number;
  height: number;
  steps: number;
  cfgScale: number;
  sampler?: string;
}

export interface RuntimeDefaults {
  backend: string;
  paramsBackend: string;
  offloadToCpu: boolean;
  diffusionFlashAttention: boolean;
  mmap: boolean;
  streamLayers: boolean;
  maxVramGiB: number;
}

export interface ModelProfile {
  family: string;
  assets: Record<string, string>;
  quality: Record<"draft" | "balanced" | "final", QualityDefaults>;
  runtime: Record<"default" | "lowVram" | "rtx4090FullSpeed", RuntimeDefaults>;
}

export const modelProfiles: Record<string, ModelProfile>;
export const errorCodes: Record<string, string>;
export const requestModes: Record<string, string>;

export class TcxSdError extends Error {
  code: string;
  remediationHints: string[];
  details?: unknown;
}

export interface CommonOptions {
  model?: string;
  modelRoot?: string;
  modelDir?: string;
  nativeDir?: string;
  serverExecutable?: string;
  host?: string;
  port?: number;
  runtimePreset?: "default" | "lowVram" | "rtx4090FullSpeed" | string;
  backend?: string;
  paramsBackend?: string;
  offloadToCpu?: boolean;
  diffusionFlashAttention?: boolean;
  mmap?: boolean;
  streamLayers?: boolean;
  maxVramGiB?: number;
  loraModelDir?: string;
}

export interface ImageOptions extends CommonOptions {
  requestMode?: string;
  prompt?: string;
  negativePrompt?: string;
  quality?: "draft" | "balanced" | "final" | string;
  width?: number;
  height?: number;
  steps?: number;
  seed?: number;
  cfgScale?: number;
  sampler?: string;
  strength?: number;
  controlStrength?: number;
  batchCount?: number;
  initImage?: string;
  maskImage?: string;
  controlImage?: string;
  sourceImage?: string;
  upscaleFactor?: number;
  loras?: Array<{ path: string; weight?: number; multiplier?: number; isHighNoise?: boolean; is_high_noise?: boolean }>;
  output?: string;
  sidecar?: string;
  outputRoot?: string;
  tempRoot?: string;
  cacheRoot?: string;
  timeoutMs?: number;
  pollMs?: number;
  signal?: AbortSignal;
  manageServer?: boolean;
  metadata?: Record<string, string>;
}

export interface GenerationSessionOptions extends ImageOptions {
  id?: string;
  project?: GenerationProject;
  projectRoot?: string;
  projectName?: string;
  root?: string;
  name?: string;
  logRoot?: string;
  inputRoot?: string;
  executionMode?: string;
}

export function classifyError(message?: string): string;
export function errorPayload(message?: string, code?: string): { code: string; message: string; remediation_hints: string[] };
export function resolveModelDir(modelId: string, options?: CommonOptions): string;
export function resolveServerExecutable(options?: CommonOptions): string;
export function resolveStorageRoots(options?: { cwd?: string; outputRoot?: string; tempRoot?: string; cacheRoot?: string }): {
  outputRoot: string;
  tempRoot: string;
  cacheRoot: string;
};
export function cleanupStorage(options?: Record<string, unknown>): Promise<string[]>;
export function normalizeLoraPath(value: string, options?: { loraModelDir?: string }): string;
export function listLoras(options?: { loraModelDir?: string; recursive?: boolean }): Promise<Array<{
  name: string;
  filename: string;
  path: string;
  relativePath: string;
  sizeBytes: number;
  modifiedMs: number;
}>>;
export function buildServerArgs(options?: CommonOptions): string[];
export function createTextToImageRequest(options?: ImageOptions): ImageOptions;
export function createImageToImageRequest(options?: ImageOptions): ImageOptions;
export function createInpaintRequest(options?: ImageOptions): ImageOptions;
export function createControlNetRequest(options?: ImageOptions): ImageOptions;
export function createLoraStackRequest(options?: ImageOptions): ImageOptions;
export function createRefineRequest(options?: ImageOptions): ImageOptions;
export function createUpscaleRequest(options?: ImageOptions): ImageOptions;
export function getBackendCapabilities(options?: Record<string, unknown>): Record<string, boolean>;
export function assertRequestSupported(options?: ImageOptions, capabilityOptions?: Record<string, unknown>): true;
export function buildImageRequest(options?: ImageOptions): Record<string, unknown>;
export function extractImageBase64(jobState: unknown): string;
export function startServer(options?: CommonOptions & { cwd?: string; logStream?: NodeJS.WritableStream }): ChildProcess;
export function waitForServer(options?: CommonOptions & { timeoutMs?: number; pollMs?: number }): Promise<true>;
export function submitImageJob(options?: ImageOptions & { body?: Record<string, unknown> }): Promise<Record<string, unknown>>;
export function cancelImageJob(job: { id?: string; poll_url?: string }, options?: CommonOptions): Promise<Record<string, unknown>>;
export function pollImageJob(job: { id?: string; poll_url?: string }, options?: ImageOptions): Promise<Record<string, unknown>>;
export function assessResultQuality(sidecar: Record<string, unknown>): { ok: boolean; error_codes: string[]; warning_codes: string[] };
export function buildSidecar(args?: Record<string, unknown>): Record<string, unknown>;
export function writeSidecar(sidecarPath: string, sidecar: Record<string, unknown>): Promise<string>;

export class TcxSdServerSession {
  constructor(options?: ImageOptions & { reuseServer?: boolean });
  start(): Promise<this>;
  generate(options?: ImageOptions): Promise<{ ok: boolean; outputPath: string; sidecarPath: string; status: string; serverJobId: string }>;
  close(): void;
}

export class GenerationProject {
  constructor(options?: { root?: string; name?: string; outputRoot?: string; tempRoot?: string; cacheRoot?: string; logRoot?: string; inputRoot?: string });
  name: string;
  root: string;
  outputRoot: string;
  tempRoot: string;
  cacheRoot: string;
  logRoot: string;
  inputRoot: string;
  storageOptions(): { outputRoot: string; tempRoot: string; cacheRoot: string };
  outputPath(label: string, extension?: string): string;
  sidecarPath(label: string): string;
  artifact(label: string, metadata?: Record<string, string>): GenerationArtifact;
}

export class GenerationArtifact {
  constructor(options?: { id?: string; outputPath?: string; sidecarPath?: string; parentSidecarPath?: string; metadata?: Record<string, string> });
  id: string;
  outputPath: string;
  sidecarPath: string;
  parentSidecarPath: string;
  metadata: Record<string, string>;
  static fromResult(result?: Record<string, unknown>, sidecar?: Record<string, unknown>): GenerationArtifact;
}

export class GenerationSession {
  constructor(options?: GenerationSessionOptions);
  id: string;
  model: string;
  profile: ModelProfile;
  runtimePreset: string;
  project: GenerationProject;
  options: GenerationSessionOptions;
  capabilities: Record<string, boolean>;
  request(quality?: string, options?: ImageOptions): ImageOptions;
  artifact(label: string, metadata?: Record<string, string>): GenerationArtifact;
  supports(request: ImageOptions): boolean;
  unsupportedReason(request: ImageOptions): string;
  serverSession(options?: ImageOptions): TcxSdServerSession;
  start(options?: ImageOptions): Promise<this>;
  generate(options?: ImageOptions): Promise<{ ok: boolean; outputPath: string; sidecarPath: string; status: string; serverJobId: string }>;
  runBatch(batch: BatchJob, options?: ImageOptions): Promise<Array<{ ok: boolean; outputPath: string; sidecarPath: string; status: string; serverJobId: string }>>;
  close(): void;
}

export class BatchJob {
  constructor(options?: string | { label?: string; baseRequest?: ImageOptions; requests?: ImageOptions[] });
  label: string;
  baseRequest: ImageOptions | null;
  requests: ImageOptions[];
  add(request: ImageOptions): this;
  seedSweep(seeds?: number[], baseRequest?: ImageOptions): this;
}

export function createServerSession(options?: ImageOptions & { reuseServer?: boolean }): TcxSdServerSession;
export function createGenerationProject(options?: ConstructorParameters<typeof GenerationProject>[0]): GenerationProject;
export function createGenerationSession(options?: GenerationSessionOptions): GenerationSession;
export function createBatchJob(options?: ConstructorParameters<typeof BatchJob>[0]): BatchJob;
export function createVariantJob(artifact: GenerationArtifact, options?: ImageOptions): { artifact: GenerationArtifact; request: ImageOptions };
export function runBatchJob(batch: BatchJob, options?: ImageOptions & { project?: GenerationProject }): Promise<Array<{ ok: boolean; outputPath: string; sidecarPath: string; status: string; serverJobId: string }>>;
export function runTextToImage(options?: ImageOptions): Promise<{ ok: boolean; outputPath: string; sidecarPath: string; status: string; serverJobId: string }>;
export function runJsonJob(jobPath: string, overrides?: ImageOptions): Promise<{ ok: boolean; outputPath: string; sidecarPath: string; status: string; serverJobId: string }>;

export const promptPacks: {
  ideogram4Poster(options?: { subject?: string; visibleText?: string; language?: string }): {
    prompt_json: Record<string, unknown>;
    negative_prompt: string;
    metadata: Record<string, string>;
  };
  productShot(options?: { subject?: string; language?: string }): { prompt: string; negative_prompt: string; metadata: Record<string, string> };
  wideScene(options?: { subject?: string; language?: string }): { prompt: string; negative_prompt: string; metadata: Record<string, string> };
  gameAsset(options?: { subject?: string; language?: string }): { prompt: string; negative_prompt: string; metadata: Record<string, string> };
  uiMockup(options?: { subject?: string; language?: string }): { prompt: string; negative_prompt: string; metadata: Record<string, string> };
};

export const canvasPresets: Record<string, { width: number; height: number; label: string }>;
export const stylePresets: Record<string, { promptPack: string; preferredModel: string }>;
export function routeModelForIntent(intent?: string, options?: ImageOptions & { visibleText?: string }): string;
