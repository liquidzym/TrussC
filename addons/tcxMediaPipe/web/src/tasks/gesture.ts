import { GestureRecognizer } from "@mediapipe/tasks-vision";
import { createWithFallback } from "./common";
import type { Delegate } from "../types";

type CategoryLike = {
  categoryName?: string;
  displayName?: string;
  score?: number;
};

function normalizedLimit(value: number): number {
  return Math.max(1, Math.floor(value));
}

export async function createGesture(delegate: Delegate, maxGestures: number) {
  return createWithFallback(delegate, (activeDelegate, vision) =>
    GestureRecognizer.createFromOptions(vision, {
      baseOptions: {
        modelAssetPath: "/models/gesture_recognizer.task",
        delegate: activeDelegate
      },
      runningMode: "VIDEO",
      numHands: normalizedLimit(maxGestures)
    })
  );
}

export function serializeGestureResult(result: any) {
  const handedness = result.handedness ?? result.handednesses ?? [];
  const gestures = (result.gestures ?? []).map((candidates: unknown, index: number) => {
    const category = Array.isArray(candidates) ? (candidates[0] as CategoryLike | undefined) : undefined;
    const handednessCategory = handedness?.[index]?.[0] as CategoryLike | undefined;
    return {
      handedness: handednessCategory?.categoryName ?? "",
      handednessScore: handednessCategory?.score ?? 0,
      categoryName: category?.categoryName ?? "",
      displayName: category?.displayName ?? "",
      score: category?.score ?? 0,
      landmarks: result.landmarks?.[index] ?? [],
      worldLandmarks: result.worldLandmarks?.[index] ?? []
    };
  });
  return { gestures };
}
