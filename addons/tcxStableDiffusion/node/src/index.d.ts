/// <reference types="node" />

import type { ChildProcess } from "node:child_process";

export const addonRoot: string;
export const defaultExampleRoot: string;
export const defaultNativeDir: string;
export const defaultModelRoot: string;

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
}

export interface ImageOptions extends CommonOptions {
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
export function buildServerArgs(options?: CommonOptions): string[];
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

export function createServerSession(options?: ImageOptions & { reuseServer?: boolean }): TcxSdServerSession;
export function runTextToImage(options?: ImageOptions): Promise<{ ok: boolean; outputPath: string; sidecarPath: string; status: string; serverJobId: string }>;
export function runJsonJob(jobPath: string, overrides?: ImageOptions): Promise<{ ok: boolean; outputPath: string; sidecarPath: string; status: string; serverJobId: string }>;

export const promptPacks: {
  ideogram4Poster(options?: { subject?: string; visibleText?: string; language?: string }): {
    prompt_json: Record<string, unknown>;
    negative_prompt: string;
    metadata: Record<string, string>;
  };
};
