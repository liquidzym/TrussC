import { Bridge } from "./bridge";
import { getVideoTrack, hasVideoFrame, startCamera, type CameraStage } from "./camera";
import { normalizeConfig } from "./config";
import { RateCounter } from "./stats";
import type { RuntimeConfig, RuntimeGpuInfo, RuntimeStatus, TaskName } from "./types";

const video = document.getElementById("camera") as HTMLVideoElement;
const canvas = document.getElementById("preview") as HTMLCanvasElement;
const status = document.getElementById("status") as HTMLDivElement;
const statusTitle = document.getElementById("status-title") as HTMLParagraphElement;
const statusDetail = document.getElementById("status-detail") as HTMLParagraphElement;
const context = canvas.getContext("2d");
const FALLBACK_FRAME_TIMEOUT_MS = 1500;
const RESULT_BUFFER_LIMIT_BYTES = 192 * 1024;

let config = normalizeConfig({});
let cameraStarted = false;
let lastFrameMs = 0;
let running = false;
let sourceFPS = 0;
let processingCanvas: HTMLCanvasElement | null = null;
let processingContext: CanvasRenderingContext2D | null = null;
let cameraStream: MediaStream | null = null;
let frameProcessing = false;
let fallbackFrameBusy = false;
let fallbackActiveReported = false;
let fallbackFailureReportedAt = 0;
let skipTrackProcessor = false;
let skipImageCapture = false;

type FrameSourceMode = "video" | "track_processor" | "image_capture" | "none";
type TrackFrame = ImageBitmapSource & { close?: () => void };
type TrackFrameReader = ReadableStreamDefaultReader<TrackFrame>;
type TrackProcessorLike = { readable: ReadableStream<TrackFrame> };
type TrackProcessorCtor = new (init: { track: MediaStreamTrack }) => TrackProcessorLike;
type ImageCaptureLike = { grabFrame(): Promise<ImageBitmap> };
type ImageCaptureCtor = new (track: MediaStreamTrack) => ImageCaptureLike;
type FrameSourceWorkerStatus = {
  type: "status";
  stage: string;
  title: string;
  detail: string;
};
type FrameSourceWorkerFrame = {
  type: "frame";
  timestampMs: number;
  frame: ImageBitmap;
};
type FrameSourceWorkerMessage = FrameSourceWorkerStatus | FrameSourceWorkerFrame;

let frameSourceMode: FrameSourceMode = "none";
let trackFrameReader: TrackFrameReader | null = null;
let imageCapture: ImageCaptureLike | null = null;
let frameSourceWorker: Worker | null = null;
let frameSourceWorkerStarted = false;
let frameSourceWorkerFailed = false;
let pendingWorkerFrame: ImageBitmap | null = null;
let pendingWorkerFrameMs = 0;

const TASK_NAMES: readonly TaskName[] = ["hand", "pose", "face", "gesture"];
const bridge = new Bridge();
const workers: Partial<Record<TaskName, Worker>> = {};
const modelStatus: Record<TaskName, boolean> = { hand: false, pose: false, face: false, gesture: false };
const resultStatus: Record<TaskName, boolean> = { hand: false, pose: false, face: false, gesture: false };
const sourceRate = new RateCounter();
const gpuInfo = detectGpuInfo();
let cameraReady = false;
let pipelineReadyReported = false;
let currentActiveDelegate: RuntimeStatus["activeDelegate"] = config.delegate;

function setStatus(state: "idle" | "ready" | "error", title: string, detail: string): void {
  status.dataset.state = state;
  statusTitle.textContent = title;
  statusDetail.textContent = detail;
}

function bridgePort(): number {
  const params = new URLSearchParams(window.location.search);
  return Number(params.get("bridgePort") ?? "0");
}

function detectGpuInfo(): RuntimeGpuInfo {
  const canvas = document.createElement("canvas");
  const gl =
    canvas.getContext("webgl2", { powerPreference: "high-performance" }) ??
    canvas.getContext("webgl", { powerPreference: "high-performance" });
  if (!gl) {
    return {};
  }

  const debugInfo = gl.getExtension("WEBGL_debug_renderer_info");
  const vendor = debugInfo ? gl.getParameter(debugInfo.UNMASKED_VENDOR_WEBGL) : gl.getParameter(gl.VENDOR);
  const renderer = debugInfo ? gl.getParameter(debugInfo.UNMASKED_RENDERER_WEBGL) : gl.getParameter(gl.RENDERER);
  return {
    webglVendor: typeof vendor === "string" ? vendor : "",
    webglRenderer: typeof renderer === "string" ? renderer : "",
    webglVersion: String(gl.getParameter(gl.VERSION) ?? ""),
    webglShadingLanguageVersion: String(gl.getParameter(gl.SHADING_LANGUAGE_VERSION) ?? "")
  };
}

function effectiveProcessingSize(sourceWidth = video.videoWidth || config.inputWidth, sourceHeight = video.videoHeight || config.inputHeight): { width: number; height: number } {
  return {
    width: config.processingWidth > 0 ? config.processingWidth : sourceWidth,
    height: config.processingHeight > 0 ? config.processingHeight : sourceHeight
  };
}

function enrichStatus(message: RuntimeStatus): RuntimeStatus {
  const size = effectiveProcessingSize();
  return {
    ...message,
    gpu: gpuInfo,
    processingWidth: size.width,
    processingHeight: size.height
  };
}

function sendRuntimeStatus(message: Partial<RuntimeStatus>): void {
  const readiness = readinessSnapshot();
  const ready = message.reason ? false : message.ready ?? readiness.pipelineReady;
  bridge.send(enrichStatus({
    type: "runtime_status",
    ready,
    ...readiness,
    activeDelegate: currentActiveDelegate ?? config.delegate,
    wasmPath: "/wasm",
    models: { ...modelStatus },
    ...message
  }));
}

function reportCameraStage(stage: CameraStage): void {
  setStatus("idle", stage.title, stage.detail);
  sendRuntimeStatus({
    stage: stage.stage,
    detail: `${stage.title}: ${stage.detail}`
  });
}

function compactGpuLabel(): string {
  const renderer = gpuInfo.webglRenderer || gpuInfo.webglVendor || "unknown GPU";
  return renderer.length > 80 ? `${renderer.slice(0, 77)}...` : renderer;
}

function enabledTasks(): TaskName[] {
  return TASK_NAMES.filter((task) => config.tasks[task]);
}

function readinessSnapshot(): Pick<RuntimeStatus, "cameraReady" | "modelReady" | "pipelineReady"> {
  const tasks = enabledTasks();
  const modelReady = tasks.every((task) => modelStatus[task]);
  const resultsReady = tasks.every((task) => resultStatus[task]);
  return {
    cameraReady,
    modelReady,
    pipelineReady: cameraReady && modelReady && resultsReady
  };
}

function taskForResultType(type: unknown): TaskName | null {
  if (type === "hand_result") {
    return "hand";
  }
  if (type === "pose_result") {
    return "pose";
  }
  if (type === "face_result") {
    return "face";
  }
  if (type === "gesture_result") {
    return "gesture";
  }
  return null;
}

function statusTaskLabel(): string {
  const enabled = enabledTasks();
  return enabled.length > 0 ? enabled.join(", ") : "none";
}

function reportPipelineReadyIfNeeded(): void {
  const readiness = readinessSnapshot();
  if (!readiness.pipelineReady || pipelineReadyReported) {
    return;
  }
  pipelineReadyReported = true;
  setStatus(
    "ready",
    "MediaPipe running",
    `Camera active. Delegate: ${currentActiveDelegate ?? config.delegate}. Tasks: ${statusTaskLabel()}. GPU: ${compactGpuLabel()}.`
  );
  sendRuntimeStatus({
    ready: true,
    stage: "running",
    detail: `MediaPipe running: tasks=${statusTaskLabel()}, GPU=${compactGpuLabel()}.`
  });
}

type NormalizedLandmark = {
  x?: number;
  [key: string]: unknown;
};

function mirrorLandmarkList(landmarks: unknown): unknown {
  if (!config.mirror || !Array.isArray(landmarks)) {
    return landmarks;
  }

  return landmarks.map((point) => {
    if (!point || typeof point !== "object") {
      return point;
    }
    const landmark = point as NormalizedLandmark;
    if (typeof landmark.x !== "number") {
      return point;
    }
    return {
      ...landmark,
      x: 1.0 - landmark.x
    };
  });
}

function mirrorHandedness(value: unknown): unknown {
  if (!config.mirror) {
    return value;
  }
  if (value === "Left") {
    return "Right";
  }
  if (value === "Right") {
    return "Left";
  }
  return value;
}

function mirrorResultMessage(message: unknown): unknown {
  if (!config.mirror || !message || typeof message !== "object") {
    return message;
  }

  const result = message as Record<string, unknown>;
  if (result.type === "hand_result" && Array.isArray(result.hands)) {
    return {
      ...result,
      hands: result.hands.map((hand) => {
        if (!hand || typeof hand !== "object") {
          return hand;
        }
        const handResult = hand as Record<string, unknown>;
        return {
          ...handResult,
          handedness: mirrorHandedness(handResult.handedness),
          landmarks: mirrorLandmarkList(handResult.landmarks)
        };
      })
    };
  }

  if (result.type === "pose_result") {
    return {
      ...result,
      poses: Array.isArray(result.poses)
        ? result.poses.map((pose) => {
            if (!pose || typeof pose !== "object") {
              return pose;
            }
            const poseResult = pose as Record<string, unknown>;
            return {
              ...poseResult,
              landmarks: mirrorLandmarkList(poseResult.landmarks)
            };
          })
        : result.poses,
      landmarks: mirrorLandmarkList(result.landmarks)
    };
  }

  if (result.type === "face_result" && Array.isArray(result.faces)) {
    return {
      ...result,
      faces: result.faces.map((face) => {
        if (!face || typeof face !== "object") {
          return face;
        }
        const faceResult = face as Record<string, unknown>;
        return {
          ...faceResult,
          landmarks: mirrorLandmarkList(faceResult.landmarks)
        };
      })
    };
  }

  if (result.type === "gesture_result" && Array.isArray(result.gestures)) {
    return {
      ...result,
      gestures: result.gestures.map((gesture) => {
        if (!gesture || typeof gesture !== "object") {
          return gesture;
        }
        const gestureResult = gesture as Record<string, unknown>;
        return {
          ...gestureResult,
          handedness: mirrorHandedness(gestureResult.handedness),
          landmarks: mirrorLandmarkList(gestureResult.landmarks)
        };
      })
    };
  }

  return message;
}

function frameSourceLabel(): string {
  if (frameSourceMode === "track_processor") {
    return "MediaStreamTrackProcessor";
  }
  if (frameSourceMode === "image_capture") {
    return "ImageCapture";
  }
  if (frameSourceMode === "video") {
    return "HTMLVideoElement";
  }
  return "no frame source";
}

function browserFrameApis(): { TrackProcessor?: TrackProcessorCtor; ImageCaptureApi?: ImageCaptureCtor } {
  const runtime = window as Window & {
    MediaStreamTrackProcessor?: TrackProcessorCtor;
    ImageCapture?: ImageCaptureCtor;
  };
  return {
    TrackProcessor: runtime.MediaStreamTrackProcessor,
    ImageCaptureApi: runtime.ImageCapture
  };
}

function setupFallbackFrameSource(stream: MediaStream): boolean {
  if (trackFrameReader || imageCapture) {
    return true;
  }

  const track = getVideoTrack(stream);
  if (!track) {
    setStatus("error", "Camera frame source failed", "getUserMedia returned no video track.");
    sendRuntimeStatus({
      stage: "camera_no_video_track",
      detail: "Camera stream live, but getUserMedia returned no video track."
    });
    return false;
  }

  const { TrackProcessor, ImageCaptureApi } = browserFrameApis();
  if (TrackProcessor && !skipTrackProcessor) {
    try {
      const processor = new TrackProcessor({ track });
      trackFrameReader = processor.readable.getReader();
      frameSourceMode = "track_processor";
      setStatus("idle", "Camera track reader", "Reading frames directly from MediaStreamTrack.");
      sendRuntimeStatus({
        stage: "camera_track_processor",
        detail: "Camera stream live: reading frames with MediaStreamTrackProcessor."
      });
      return true;
    } catch (error) {
      const reason = error instanceof Error ? error.message : "MediaStreamTrackProcessor setup failed";
      sendRuntimeStatus({
        stage: "camera_track_processor_error",
        detail: `MediaStreamTrackProcessor failed: ${reason}`
      });
    }
  }

  if (ImageCaptureApi && !skipImageCapture) {
    try {
      imageCapture = new ImageCaptureApi(track);
      frameSourceMode = "image_capture";
      setStatus("idle", "Camera image capture", "Reading frames directly from MediaStreamTrack.");
      sendRuntimeStatus({
        stage: "camera_image_capture",
        detail: "Camera stream live: reading frames with ImageCapture.grabFrame()."
      });
      return true;
    } catch (error) {
      const reason = error instanceof Error ? error.message : "ImageCapture setup failed";
      sendRuntimeStatus({
        stage: "camera_image_capture_error",
        detail: `ImageCapture failed: ${reason}`
      });
    }
  }

  frameSourceMode = "none";
  setStatus("error", "Camera frame source unavailable", "The camera track is live, but this CEF build exposes no track frame reader.");
  sendRuntimeStatus({
    stage: "camera_no_track_frame_api",
    detail: "Camera stream live, but neither MediaStreamTrackProcessor nor ImageCapture is available."
  });
  return false;
}

function setupWorkerFrameSource(stream: MediaStream): boolean {
  if (frameSourceWorkerStarted || frameSourceWorker) {
    return true;
  }

  const track = getVideoTrack(stream);
  if (!track) {
    return false;
  }

  const workerTrack = track.clone();
  const worker = new Worker(new URL("./workers/frame-source.worker.ts", import.meta.url), { type: "module" });
  frameSourceWorker = worker;
  frameSourceWorkerStarted = true;
  frameSourceWorkerFailed = false;

  worker.onmessage = (event: MessageEvent<FrameSourceWorkerMessage>) => {
    const message = event.data;
    if (message.type === "status") {
      if (
        message.stage === "camera_worker_no_track_processor" ||
        message.stage === "camera_worker_setup_error" ||
        message.stage === "camera_worker_reader_error" ||
        message.stage === "camera_worker_reader_ended"
      ) {
        frameSourceWorkerFailed = true;
        if (cameraStream) {
          setupFallbackFrameSource(cameraStream);
        }
      } else {
        frameSourceMode = "track_processor";
      }

      setStatus(frameSourceWorkerFailed ? "idle" : "ready", message.title, message.detail);
      sendRuntimeStatus({
        stage: message.stage,
        detail: `${message.title}: ${message.detail}`
      });
      return;
    }

    if (pendingWorkerFrame) {
      pendingWorkerFrame.close();
    }
    pendingWorkerFrame = message.frame;
    pendingWorkerFrameMs = message.timestampMs;
  };

  worker.onerror = (event) => {
    frameSourceWorkerFailed = true;
    const reason = event.message || "Camera frame-source worker failed.";
    setStatus("error", "Camera frame-source worker failed", reason);
    sendRuntimeStatus({
      stage: "camera_worker_error",
      detail: reason
    });
  };

  try {
    worker.postMessage({ type: "start", track: workerTrack }, [workerTrack as unknown as Transferable]);
    setStatus("idle", "Camera worker starting", "Testing MediaStreamTrackProcessor in Dedicated Worker.");
    sendRuntimeStatus({
      stage: "camera_worker_starting",
      detail: "Testing MediaStreamTrackProcessor in Dedicated Worker."
    });
    return true;
  } catch (error) {
    frameSourceWorkerFailed = true;
    worker.terminate();
    frameSourceWorker = null;
    const reason = error instanceof Error ? error.message : "Could not transfer camera track to Dedicated Worker.";
    setStatus("idle", "Camera worker unavailable", reason);
    sendRuntimeStatus({
      stage: "camera_worker_transfer_error",
      detail: reason
    });
    try {
      workerTrack.stop();
    } catch {
      // Ignore cleanup errors from detached tracks.
    }
    return false;
  }
}

function reportFallbackFailure(error: unknown): void {
  const now = performance.now();
  if (now - fallbackFailureReportedAt < 1000) {
    return;
  }
  fallbackFailureReportedAt = now;
  const reason = error instanceof Error ? error.message : "Failed to read a camera frame from MediaStreamTrack";
  setStatus("idle", "Camera frame pending", reason);
  sendRuntimeStatus({
    stage: "camera_fallback_frame_error",
    detail: `${frameSourceLabel()}: ${reason}`
  });
}

function timeoutReason(source: string): string {
  return `${source} did not produce a frame within ${FALLBACK_FRAME_TIMEOUT_MS} ms.`;
}

async function readTrackFrame(): Promise<ImageBitmap | null> {
  if (!trackFrameReader) {
    return null;
  }

  const reader = trackFrameReader;
  let timeoutId = 0;
  const timeout = new Promise<"timeout">((resolve) => {
    timeoutId = window.setTimeout(() => resolve("timeout"), FALLBACK_FRAME_TIMEOUT_MS);
  });
  try {
    const result = await Promise.race([reader.read(), timeout]);
    if (result === "timeout") {
      skipTrackProcessor = true;
      trackFrameReader = null;
      void reader.cancel().catch(() => {});
      reportFallbackFailure(new Error(timeoutReason("MediaStreamTrackProcessor")));
      if (cameraStream) {
        setupFallbackFrameSource(cameraStream);
      }
      return null;
    }

    if (result.done || !result.value) {
      trackFrameReader = null;
      frameSourceMode = "none";
      return null;
    }

    const source = result.value;
    try {
      return await createImageBitmap(source as ImageBitmapSource);
    } finally {
      source.close?.();
    }
  } finally {
    if (timeoutId !== 0) {
      window.clearTimeout(timeoutId);
    }
  }
}

async function grabImageCaptureFrame(): Promise<ImageBitmap | null> {
  if (!imageCapture) {
    return null;
  }

  let timeoutId = 0;
  const timeout = new Promise<"timeout">((resolve) => {
    timeoutId = window.setTimeout(() => resolve("timeout"), FALLBACK_FRAME_TIMEOUT_MS);
  });
  try {
    const result = await Promise.race([imageCapture.grabFrame(), timeout]);
    if (result === "timeout") {
      skipImageCapture = true;
      imageCapture = null;
      frameSourceMode = "none";
      reportFallbackFailure(new Error(timeoutReason("ImageCapture.grabFrame()")));
      return null;
    }
    return result;
  } finally {
    if (timeoutId !== 0) {
      window.clearTimeout(timeoutId);
    }
  }
}

async function captureFallbackFrame(): Promise<ImageBitmap | null> {
  if (fallbackFrameBusy) {
    return null;
  }
  fallbackFrameBusy = true;
  try {
    if (trackFrameReader) {
      return await readTrackFrame();
    }

    if (imageCapture) {
      return await grabImageCaptureFrame();
    }
  } catch (error) {
    reportFallbackFailure(error);
  } finally {
    fallbackFrameBusy = false;
  }
  return null;
}

function createWorker(task: TaskName): Worker {
  if (workers[task]) {
    return workers[task]!;
  }
  const worker =
    task === "hand"
      ? new Worker(new URL("./workers/hand.worker.ts", import.meta.url), { type: "module" })
      : task === "pose"
        ? new Worker(new URL("./workers/pose.worker.ts", import.meta.url), { type: "module" })
        : task === "face"
          ? new Worker(new URL("./workers/face.worker.ts", import.meta.url), { type: "module" })
          : new Worker(new URL("./workers/gesture.worker.ts", import.meta.url), { type: "module" });

  worker.onmessage = (event: MessageEvent) => {
    const message = event.data as RuntimeStatus | Record<string, unknown>;
    if (message.type === "runtime_status") {
      const status = message as RuntimeStatus;
      if (status.activeDelegate) {
        currentActiveDelegate = status.activeDelegate;
      }
      if (status.models) {
        for (const name of TASK_NAMES) {
          modelStatus[name] = modelStatus[name] || Boolean(status.models[name]);
        }
      }
      if (status.ready) {
        reportPipelineReadyIfNeeded();
      } else if (status.reason) {
        setStatus("error", "MediaPipe runtime error", status.reason);
      }
      const readiness = readinessSnapshot();
      bridge.send(enrichStatus({
        ...status,
        ready: status.reason ? false : readiness.pipelineReady,
        ...readiness,
        models: { ...modelStatus }
      }));
    } else {
      const taskName = taskForResultType(message.type);
      if (taskName) {
        resultStatus[taskName] = true;
        reportPipelineReadyIfNeeded();
      }
      bridge.send(mirrorResultMessage(message), { dropIfBufferedBytes: RESULT_BUFFER_LIMIT_BYTES });
    }
  };
  worker.onerror = (event) => {
    setStatus("error", `${task} worker error`, event.message || "Worker failed to initialize.");
    bridge.send(enrichStatus({
      type: "runtime_status",
      ready: false,
      activeDelegate: config.delegate,
      reason: event.message || `${task} worker failed`,
      wasmPath: "/wasm",
      models: { ...modelStatus }
    }));
  };

  workers[task] = worker;
  return worker;
}

async function applyConfig(nextConfig: RuntimeConfig): Promise<void> {
  config = normalizeConfig(nextConfig);
  currentActiveDelegate = config.delegate;
  pipelineReadyReported = false;
  for (const task of TASK_NAMES) {
    if (config.tasks[task]) {
      resultStatus[task] = false;
      if (!workers[task]) {
        modelStatus[task] = false;
      }
    }
  }
  video.classList.toggle("mirrored", config.mirror);

  if (config.inputMode !== "WebCamera") {
    setStatus("error", "Unsupported input mode", "ExternalFrame input is not implemented in tcxMediaPipe 0.1.0");
    sendRuntimeStatus({
      reason: "ExternalFrame input is not implemented in tcxMediaPipe 0.1.0",
    });
    return;
  }

  let workersConfigured = false;
  const configureEnabledWorkers = () => {
    for (const task of TASK_NAMES) {
      if (config.tasks[task]) {
        createWorker(task).postMessage({
          type: "config",
          delegate: config.delegate,
          maxHands: config.maxHands,
          maxPoses: config.maxPoses,
          maxFaces: config.maxFaces,
          maxGestures: config.maxGestures,
          outputFaceBlendshapes: config.outputFaceBlendshapes,
          outputFaceTransformationMatrix: config.outputFaceTransformationMatrix
        });
      }
    }
    workersConfigured = true;
  };

  if (!cameraStarted) {
    setStatus("idle", "Requesting camera", "Allow camera access when macOS asks for permission.");
    const cameraStart = startCamera(video, config.inputWidth, config.inputHeight, reportCameraStage);
    configureEnabledWorkers();
    const cameraResult = await cameraStart;
    cameraStream = cameraResult.stream;
    cameraStarted = true;
    if (cameraResult.firstFrameReady) {
      frameSourceMode = "video";
      setStatus("idle", "Camera active", "Loading MediaPipe models.");
      sendRuntimeStatus({
        stage: "models_loading",
        detail: "Camera active: loading MediaPipe models."
      });
    } else if (setupWorkerFrameSource(cameraResult.stream)) {
      setStatus("idle", "Camera worker starting", "Loading MediaPipe models while testing worker frame capture.");
      sendRuntimeStatus({
        stage: "models_loading",
        detail: "Camera video element has no first frame; testing Dedicated Worker frame capture."
      });
    } else if (setupFallbackFrameSource(cameraResult.stream)) {
      setStatus("idle", "Camera track active", `Loading MediaPipe models with ${frameSourceLabel()} frames.`);
      sendRuntimeStatus({
        stage: "models_loading",
        detail: `Camera video element has no first frame; loading models with ${frameSourceLabel()} frames.`
      });
    } else {
      setStatus("idle", "Camera stream live", "Waiting for the first video frame.");
      sendRuntimeStatus({
        stage: "camera_waiting_first_frame",
        detail: "Camera stream live: waiting for the first video frame."
      });
    }
  }

  if (!workersConfigured) {
    configureEnabledWorkers();
  }

  if (!running) {
    running = true;
    requestAnimationFrame(loop);
  }
}

async function createProcessedBitmap(source: ImageBitmapSource, sourceWidth: number, sourceHeight: number): Promise<ImageBitmap> {
  const size = effectiveProcessingSize(sourceWidth, sourceHeight);
  if (size.width > 0 && size.height > 0 && (size.width !== sourceWidth || size.height !== sourceHeight)) {
    try {
      return await createImageBitmap(source, {
        resizeWidth: size.width,
        resizeHeight: size.height,
        resizeQuality: "low"
      });
    } catch {
      // Fall back to canvas scaling for CEF builds without resize options.
    }
    if (!processingCanvas) {
      processingCanvas = document.createElement("canvas");
      processingContext = processingCanvas.getContext("2d", { alpha: false });
    }
    if (!processingCanvas || !processingContext) {
      throw new Error("2D processing canvas is not available");
    }
    processingCanvas.width = size.width;
    processingCanvas.height = size.height;
    processingContext.drawImage(source as CanvasImageSource, 0, 0, size.width, size.height);
    return await createImageBitmap(processingCanvas);
  }
  return await createImageBitmap(source);
}

async function createProcessedBitmaps(
  source: ImageBitmapSource,
  sourceWidth: number,
  sourceHeight: number,
  count: number
): Promise<ImageBitmap[]> {
  if (count <= 0) {
    return [];
  }

  const size = effectiveProcessingSize(sourceWidth, sourceHeight);
  const shouldUseProcessingCanvas =
    count > 1 ||
    (size.width > 0 && size.height > 0 && (size.width !== sourceWidth || size.height !== sourceHeight));

  if (!shouldUseProcessingCanvas) {
    return [await createProcessedBitmap(source, sourceWidth, sourceHeight)];
  }

  if (!processingCanvas) {
    processingCanvas = document.createElement("canvas");
    processingContext = processingCanvas.getContext("2d", { alpha: false });
  }
  if (!processingCanvas || !processingContext) {
    throw new Error("2D processing canvas is not available");
  }

  const width = size.width > 0 ? size.width : sourceWidth;
  const height = size.height > 0 ? size.height : sourceHeight;
  processingCanvas.width = width;
  processingCanvas.height = height;
  processingContext.drawImage(source as CanvasImageSource, 0, 0, width, height);

  const frames: ImageBitmap[] = [];
  for (let i = 0; i < count; ++i) {
    frames.push(await createImageBitmap(processingCanvas));
  }
  return frames;
}

async function postSourceFrameToTasks(
  source: ImageBitmapSource,
  sourceWidth: number,
  sourceHeight: number,
  timestampMs: number
): Promise<void> {
  const tasks = enabledTasks().filter((task) => workers[task]);
  if (tasks.length === 0) {
    return;
  }

  let frames: ImageBitmap[] = [];
  try {
    frames = await createProcessedBitmaps(source, sourceWidth, sourceHeight, tasks.length);
    tasks.forEach((task, index) => {
      const worker = workers[task];
      const frame = frames[index];
      if (!worker || !frame) {
        frame?.close();
        return;
      }
      worker.postMessage({ type: "detect_video", timestampMs, sourceFPS, frame }, [frame]);
    });
  } catch (error) {
    for (const frame of frames) {
      frame.close();
    }
    const reason = error instanceof Error ? error.message : "Failed to create camera frame";
    setStatus("error", "Frame capture failed", reason);
    sendRuntimeStatus({
      reason
    });
  }
}

function drawVideoPreview(): void {
  if (!context || video.videoWidth <= 0 || video.videoHeight <= 0) {
    return;
  }
  document.body.classList.remove("use-canvas-preview");
  canvas.width = video.videoWidth;
  canvas.height = video.videoHeight;
  context.save();
  if (config.mirror) {
    context.translate(canvas.width, 0);
    context.scale(-1, 1);
  }
  context.drawImage(video, 0, 0, canvas.width, canvas.height);
  context.restore();
}

function drawBitmapPreview(bitmap: ImageBitmap): void {
  if (!context || bitmap.width <= 0 || bitmap.height <= 0) {
    return;
  }
  document.body.classList.add("use-canvas-preview");
  canvas.width = bitmap.width;
  canvas.height = bitmap.height;
  context.save();
  if (config.mirror) {
    context.translate(canvas.width, 0);
    context.scale(-1, 1);
  }
  context.drawImage(bitmap, 0, 0, canvas.width, canvas.height);
  context.restore();
}

async function processFrame(nowMs: number): Promise<void> {
  if (hasVideoFrame(video)) {
    frameSourceMode = "video";
    cameraReady = true;
    sourceFPS = sourceRate.tick(nowMs);
    drawVideoPreview();
    await postSourceFrameToTasks(video, video.videoWidth, video.videoHeight, nowMs);
    return;
  }

  if (pendingWorkerFrame) {
    const bitmap = pendingWorkerFrame;
    const timestampMs = pendingWorkerFrameMs || nowMs;
    pendingWorkerFrame = null;
    pendingWorkerFrameMs = 0;
    try {
      if (!fallbackActiveReported) {
        fallbackActiveReported = true;
        setStatus("idle", "Camera worker frames", "Frames are flowing through MediaStreamTrackProcessor in Dedicated Worker.");
        sendRuntimeStatus({
          stage: "camera_worker_frames",
          detail: "Camera frames available through MediaStreamTrackProcessor in Dedicated Worker."
        });
      }
      cameraReady = true;
      sourceFPS = sourceRate.tick(timestampMs);
      drawBitmapPreview(bitmap);
      await postSourceFrameToTasks(bitmap, bitmap.width, bitmap.height, timestampMs);
    } finally {
      bitmap.close();
    }
    return;
  }

  if (frameSourceWorkerStarted && !frameSourceWorkerFailed) {
    return;
  }

  if (!trackFrameReader && !imageCapture && cameraStream) {
    setupFallbackFrameSource(cameraStream);
  }

  const bitmap = await captureFallbackFrame();
  if (!bitmap) {
    return;
  }

  try {
    if (!fallbackActiveReported) {
      fallbackActiveReported = true;
      setStatus("idle", "Camera track frames", `Frames are flowing through ${frameSourceLabel()}.`);
      sendRuntimeStatus({
        stage: "camera_track_frames",
        detail: `Camera frames available through ${frameSourceLabel()}.`
      });
    }
    cameraReady = true;
    sourceFPS = sourceRate.tick(nowMs);
    drawBitmapPreview(bitmap);
    await postSourceFrameToTasks(bitmap, bitmap.width, bitmap.height, nowMs);
  } finally {
    bitmap.close();
  }
}

function loop(nowMs: number): void {
  const minFrameMs = 1000 / Math.max(1, config.maxFPS);
  if (cameraStarted && !frameProcessing && nowMs - lastFrameMs >= minFrameMs) {
    lastFrameMs = nowMs;
    frameProcessing = true;
    void processFrame(nowMs).finally(() => {
      frameProcessing = false;
    });
  }
  requestAnimationFrame(loop);
}

bridge.onConfig = (message) => {
  setStatus("idle", "Configuration received", "Preparing camera and MediaPipe tasks.");
  void applyConfig(message as RuntimeConfig).catch((error) => {
    const reason = error instanceof Error ? error.message : "Failed to apply MediaPipe config";
    setStatus("error", "tcxMediaPipe setup failed", reason);
    sendRuntimeStatus({
      reason,
    });
  });
};

bridge.connect(bridgePort());
setStatus("idle", "Connecting to TrussC", `WebSocket bridge port: ${bridgePort() || "unknown"}.`);
sendRuntimeStatus({});
