import type { RuntimeConfig } from "./types";

export const defaultConfig: RuntimeConfig = {
  type: "config",
  delegate: "GPU",
  inputMode: "WebCamera",
  tasks: {
    hand: true,
    pose: false,
    face: false,
    gesture: false
  },
  maxFPS: 30,
  inputWidth: 640,
  inputHeight: 480,
  processingWidth: 0,
  processingHeight: 0,
  mirror: true,
  multiPerson: true,
  maxHands: 4,
  maxPoses: 4,
  maxFaces: 4,
  maxGestures: 4
};

function normalizeLimit(value: unknown, fallback: number, multiPerson: boolean): number {
  if (!multiPerson) {
    return 1;
  }
  const numberValue = typeof value === "number" ? value : fallback;
  return Math.max(1, Math.floor(numberValue));
}

function normalizePositiveInteger(value: unknown, fallback: number): number {
  const numberValue = typeof value === "number" ? value : fallback;
  return Math.max(1, Math.floor(numberValue));
}

function normalizeNonNegativeInteger(value: unknown, fallback: number): number {
  const numberValue = typeof value === "number" ? value : fallback;
  return Math.max(0, Math.floor(numberValue));
}

export function normalizeConfig(value: Partial<RuntimeConfig>): RuntimeConfig {
  const multiPerson = value.multiPerson ?? defaultConfig.multiPerson;
  return {
    ...defaultConfig,
    ...value,
    tasks: {
      ...defaultConfig.tasks,
      ...(value.tasks ?? {})
    },
    multiPerson,
    maxFPS: normalizePositiveInteger(value.maxFPS, defaultConfig.maxFPS),
    inputWidth: normalizePositiveInteger(value.inputWidth, defaultConfig.inputWidth),
    inputHeight: normalizePositiveInteger(value.inputHeight, defaultConfig.inputHeight),
    processingWidth: normalizeNonNegativeInteger(value.processingWidth, defaultConfig.processingWidth),
    processingHeight: normalizeNonNegativeInteger(value.processingHeight, defaultConfig.processingHeight),
    maxHands: normalizeLimit(value.maxHands, defaultConfig.maxHands, multiPerson),
    maxPoses: normalizeLimit(value.maxPoses, defaultConfig.maxPoses, multiPerson),
    maxFaces: normalizeLimit(value.maxFaces, defaultConfig.maxFaces, multiPerson),
    maxGestures: normalizeLimit(value.maxGestures, defaultConfig.maxGestures, multiPerson)
  };
}
