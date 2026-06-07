import { FilesetResolver } from "@mediapipe/tasks-vision";
import type { Delegate } from "../types";

export type TaskCreation<T> = {
  task: T;
  activeDelegate: Delegate;
  fallback: boolean;
  reason?: string;
};

export async function createWithFallback<T>(
  requestedDelegate: Delegate,
  create: (delegate: Delegate, vision: Awaited<ReturnType<typeof FilesetResolver.forVisionTasks>>) => Promise<T>
): Promise<TaskCreation<T>> {
  const useModuleLoader = true;
  const vision = await FilesetResolver.forVisionTasks("/wasm", useModuleLoader);
  try {
    return {
      task: await create(requestedDelegate, vision),
      activeDelegate: requestedDelegate,
      fallback: false
    };
  } catch (error) {
    if (requestedDelegate === "CPU") {
      throw error;
    }
    return {
      task: await create("CPU", vision),
      activeDelegate: "CPU",
      fallback: true,
      reason: error instanceof Error ? error.message : "GPU initialization failed"
    };
  }
}
