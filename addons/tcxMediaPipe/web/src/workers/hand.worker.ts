import { createHand, serializeHandResult } from "../tasks/hand";
import { RateCounter } from "../stats";
import type { Delegate, WorkerInboundMessage } from "../types";

let task: Awaited<ReturnType<typeof createHand>>["task"] | null = null;
let activeDelegate: Delegate = "GPU";
let busy = false;
const inferenceRate = new RateCounter();

self.onmessage = async (event: MessageEvent<WorkerInboundMessage>) => {
  const message = event.data;
  if (message.type === "config") {
    try {
      const created = await createHand(message.delegate, message.maxHands);
      task = created.task;
      activeDelegate = created.activeDelegate;
      self.postMessage({
        type: "runtime_status",
        ready: true,
        activeDelegate,
        fallback: created.fallback,
        reason: created.reason,
        wasmPath: "/wasm",
        models: { hand: true, pose: false, face: false, gesture: false }
      });
    } catch (error) {
      self.postMessage({
        type: "runtime_status",
        ready: false,
        activeDelegate,
        fallback: activeDelegate === "CPU",
        reason: error instanceof Error ? error.message : "HandLandmarker setup failed",
        wasmPath: "/wasm",
        models: { hand: false, pose: false, face: false, gesture: false }
      });
    }
    return;
  }

  if (!task || busy) {
    message.frame.close();
    return;
  }

  busy = true;
  const started = performance.now();
  try {
    const result = task.detectForVideo(message.frame, message.timestampMs);
    const inferenceTimeMs = performance.now() - started;
    self.postMessage({
      type: "hand_result",
      timestampMs: message.timestampMs,
      inferenceTimeMs,
      stats: {
        sourceFPS: message.sourceFPS,
        inferenceFPS: inferenceRate.tick(),
        averageInferenceTimeMs: inferenceTimeMs,
        sentAtEpochMs: Date.now()
      },
      ...serializeHandResult(result)
    });
  } finally {
    message.frame.close();
    busy = false;
  }
};
