import { PoseLandmarker } from "@mediapipe/tasks-vision";
import { createWithFallback } from "./common";
import type { Delegate } from "../types";

function normalizedLimit(value: number): number {
  return Math.max(1, Math.floor(value));
}

export async function createPose(delegate: Delegate, maxPoses: number) {
  return createWithFallback(delegate, (activeDelegate, vision) =>
    PoseLandmarker.createFromOptions(vision, {
      baseOptions: {
        modelAssetPath: "/models/pose_landmarker_full.task",
        delegate: activeDelegate
      },
      runningMode: "VIDEO",
      numPoses: normalizedLimit(maxPoses),
      outputSegmentationMasks: false
    })
  );
}

export function serializePoseResult(result: any) {
  const poses = (result.landmarks ?? []).map((landmarks: unknown, index: number) => ({
    landmarks,
    worldLandmarks: result.worldLandmarks?.[index] ?? [],
    segmentationMaskAvailable: Boolean(result.segmentationMasks?.[index])
  }));
  const firstPose = poses[0];
  return {
    poses,
    landmarks: firstPose?.landmarks ?? [],
    worldLandmarks: firstPose?.worldLandmarks ?? [],
    segmentationMaskAvailable: firstPose?.segmentationMaskAvailable ?? false
  };
}
