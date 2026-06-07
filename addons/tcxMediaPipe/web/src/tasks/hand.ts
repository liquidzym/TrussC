import { HandLandmarker } from "@mediapipe/tasks-vision";
import { createWithFallback } from "./common";
import type { Delegate } from "../types";

function normalizedLimit(value: number): number {
  return Math.max(1, Math.floor(value));
}

export async function createHand(delegate: Delegate, maxHands: number) {
  return createWithFallback(delegate, (activeDelegate, vision) =>
    HandLandmarker.createFromOptions(vision, {
      baseOptions: {
        modelAssetPath: "/models/hand_landmarker.task",
        delegate: activeDelegate
      },
      runningMode: "VIDEO",
      numHands: normalizedLimit(maxHands)
    })
  );
}

export function serializeHandResult(result: any) {
  const hands = (result.landmarks ?? []).map((landmarks: unknown, index: number) => {
    const handedness = result.handednesses?.[index]?.[0];
    return {
      handedness: handedness?.categoryName ?? "",
      score: handedness?.score ?? 0,
      landmarks,
      worldLandmarks: result.worldLandmarks?.[index] ?? []
    };
  });
  return { hands };
}
