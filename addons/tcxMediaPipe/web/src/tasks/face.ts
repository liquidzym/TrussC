import { FaceLandmarker } from "@mediapipe/tasks-vision";
import { createWithFallback } from "./common";
import type { Delegate } from "../types";

function normalizedLimit(value: number): number {
  return Math.max(1, Math.floor(value));
}

export async function createFace(delegate: Delegate, maxFaces: number) {
  return createWithFallback(delegate, (activeDelegate, vision) =>
    FaceLandmarker.createFromOptions(vision, {
      baseOptions: {
        modelAssetPath: "/models/face_landmarker.task",
        delegate: activeDelegate
      },
      runningMode: "VIDEO",
      numFaces: normalizedLimit(maxFaces),
      outputFaceBlendshapes: true,
      outputFacialTransformationMatrixes: true
    })
  );
}

export function serializeFaceResult(result: any) {
  const faces = (result.faceLandmarks ?? []).map((landmarks: unknown, index: number) => {
    const categories = result.faceBlendshapes?.[index]?.categories ?? [];
    const blendshapes: Record<string, number> = {};
    for (const category of categories) {
      blendshapes[category.categoryName] = category.score;
    }
    return {
      landmarks,
      blendshapes,
      facialTransformationMatrix: result.facialTransformationMatrixes?.[index]?.data ?? []
    };
  });
  return { faces };
}
