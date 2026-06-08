import { FaceLandmarker } from "@mediapipe/tasks-vision";
import { createWithFallback } from "./common";
import type { Delegate } from "../types";

function normalizedLimit(value: number): number {
  return Math.max(1, Math.floor(value));
}

type FaceOptions = {
  outputFaceBlendshapes: boolean;
  outputFaceTransformationMatrix: boolean;
};

export async function createFace(delegate: Delegate, maxFaces: number, options: FaceOptions) {
  return createWithFallback(delegate, (activeDelegate, vision) =>
    FaceLandmarker.createFromOptions(vision, {
      baseOptions: {
        modelAssetPath: "/models/face_landmarker.task",
        delegate: activeDelegate
      },
      runningMode: "VIDEO",
      numFaces: normalizedLimit(maxFaces),
      outputFaceBlendshapes: options.outputFaceBlendshapes,
      outputFacialTransformationMatrixes: options.outputFaceTransformationMatrix
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
    const face: Record<string, unknown> = { landmarks };
    if (categories.length > 0) {
      face.blendshapes = blendshapes;
    }

    const matrix = result.facialTransformationMatrixes?.[index]?.data;
    if (matrix && matrix.length > 0) {
      face.facialTransformationMatrix = Array.from(matrix);
    }
    return face;
  });
  return { faces };
}
