import type { TxcSdWorkflow } from "./schema";

export const textToImageZhWorkflow: TxcSdWorkflow = {
  schema: "tcxsd.workflow.v1",
  id: "text-to-image.zh",
  title: "中文海报生成",
  language: "zh",
  nodes: [
    { id: "model", kind: "ModelProfile", label: "Ideogram4", x: 60, y: 90, data: { model: "ideogram4-q4_0" } },
    { id: "runtime", kind: "RuntimePreset", label: "低显存 CUDA", x: 60, y: 230, data: { runtimePreset: "lowVram", quality: "draft", width: 512, height: 512, steps: 8, cfgScale: 7 } },
    { id: "prompt", kind: "Prompt", label: "中文提示词", x: 300, y: 90, data: { prompt: "一张高端中文海报，画面中心有清晰可读文字“本地生成”，深色背景，土黄金标题，现代创作工具质感。" } },
    { id: "negative", kind: "NegativePrompt", label: "负面提示词", x: 300, y: 250, data: { negativePrompt: "低质量，模糊，错别字，不可读文字，水印，签名" } },
    { id: "generate", kind: "Generate", label: "生成", x: 540, y: 120, data: { requestMode: "text_to_image", outputName: "text_to_image_zh" } },
    { id: "quality", kind: "QualityCheck", label: "质量检查", x: 540, y: 260, data: { rejectBlank: true, rejectWrongSize: true, detectTextFailure: true } },
    { id: "save", kind: "SaveArtifact", label: "保存产物", x: 540, y: 400, data: { projectName: "workflow-web-cef" } }
  ],
  edges: [
    { id: "model-runtime", from: "model", to: "runtime", kind: "settings" },
    { id: "runtime-generate", from: "runtime", to: "generate", kind: "settings" },
    { id: "prompt-generate", from: "prompt", to: "generate", kind: "prompt" },
    { id: "negative-generate", from: "negative", to: "generate", kind: "negativePrompt" },
    { id: "generate-quality", from: "generate", to: "quality", kind: "artifact" },
    { id: "quality-save", from: "quality", to: "save", kind: "artifact" }
  ]
};

export const controlNetCannyWorkflow: TxcSdWorkflow = {
  schema: "tcxsd.workflow.v1",
  id: "controlnet-canny",
  title: "ControlNet Canny",
  language: "zh",
  nodes: [
    { id: "model", kind: "ModelProfile", label: "SD15 ControlNet", x: 60, y: 100, data: { model: "sd15-controlnet-canny" } },
    { id: "runtime", kind: "RuntimePreset", label: "默认档", x: 60, y: 240, data: { runtimePreset: "default", quality: "draft", width: 512, height: 512, steps: 12, cfgScale: 7.5 } },
    { id: "control", kind: "ControlNet", label: "Canny 引导图", x: 300, y: 100, data: { controlImage: "bin/data/inputs/control/canny-guide.png", controlStrength: 1.0 } },
    { id: "prompt", kind: "Prompt", label: "提示词", x: 300, y: 260, data: { prompt: "由 Canny 图控制的精确建筑产品场景，深色工作室，土黄金高光，边缘清晰。" } },
    { id: "generate", kind: "Generate", label: "生成", x: 540, y: 120, data: { requestMode: "control_net", outputName: "controlnet_canny" } },
    { id: "quality", kind: "QualityCheck", label: "质量检查", x: 540, y: 260, data: { rejectBlank: true, rejectWrongSize: true } },
    { id: "save", kind: "SaveArtifact", label: "保存产物", x: 540, y: 400, data: { projectName: "workflow-web-cef" } }
  ],
  edges: [
    { id: "runtime-generate", from: "runtime", to: "generate", kind: "settings" },
    { id: "control-generate", from: "control", to: "generate", kind: "control" },
    { id: "prompt-generate", from: "prompt", to: "generate", kind: "prompt" },
    { id: "generate-quality", from: "generate", to: "quality", kind: "artifact" },
    { id: "quality-save", from: "quality", to: "save", kind: "artifact" }
  ]
};

export const inpaintWorkflow: TxcSdWorkflow = {
  schema: "tcxsd.workflow.v1",
  id: "inpaint",
  title: "局部重绘修复",
  language: "zh",
  nodes: [
    { id: "model", kind: "ModelProfile", label: "FLUX.2-klein", x: 60, y: 100, data: { model: "flux2-klein-4b-q4_0" } },
    { id: "source", kind: "SourceImage", label: "源图", x: 300, y: 80, data: { path: "bin/data/inputs/inpaint/source.png" } },
    { id: "mask", kind: "MaskImage", label: "遮罩图", x: 300, y: 220, data: { path: "bin/data/inputs/inpaint/mask.png" } },
    { id: "prompt", kind: "Prompt", label: "修复提示词", x: 300, y: 360, data: { prompt: "修复高亮区域，保持温暖工作室材质和干净产品光。" } },
    { id: "generate", kind: "Generate", label: "生成", x: 540, y: 150, data: { requestMode: "inpaint", strength: 0.68, outputName: "inpaint_repair" } },
    { id: "quality", kind: "QualityCheck", label: "质量检查", x: 540, y: 290, data: { rejectBlank: true } },
    { id: "save", kind: "SaveArtifact", label: "保存产物", x: 540, y: 430, data: { projectName: "workflow-web-cef" } }
  ],
  edges: [
    { id: "source-generate", from: "source", to: "generate", kind: "image" },
    { id: "mask-generate", from: "mask", to: "generate", kind: "mask" },
    { id: "prompt-generate", from: "prompt", to: "generate", kind: "prompt" },
    { id: "generate-quality", from: "generate", to: "quality", kind: "artifact" },
    { id: "quality-save", from: "quality", to: "save", kind: "artifact" }
  ]
};

export const loraStackWorkflow: TxcSdWorkflow = {
  schema: "tcxsd.workflow.v1",
  id: "lora-stack",
  title: "LoRA 风格栈",
  language: "zh",
  nodes: [
    { id: "model", kind: "ModelProfile", label: "FLUX.2-klein", x: 60, y: 120, data: { model: "flux2-klein-4b-q4_0" } },
    { id: "prompt", kind: "Prompt", label: "提示词", x: 300, y: 100, data: { prompt: "带有精致自定义风格的本地 AI 工作流产品图。" } },
    { id: "lora", kind: "LoRAStack", label: "LoRA 栈", x: 300, y: 250, data: { loras: [{ path: "starter/style.safetensors", weight: 0.65 }] } },
    { id: "generate", kind: "Generate", label: "生成", x: 540, y: 130, data: { requestMode: "lora_stack", outputName: "lora_stack" } },
    { id: "quality", kind: "QualityCheck", label: "质量检查", x: 540, y: 270, data: { rejectBlank: true } },
    { id: "save", kind: "SaveArtifact", label: "保存产物", x: 540, y: 410, data: { projectName: "workflow-web-cef" } }
  ],
  edges: [
    { id: "prompt-generate", from: "prompt", to: "generate", kind: "prompt" },
    { id: "lora-generate", from: "lora", to: "generate", kind: "lora" },
    { id: "generate-quality", from: "generate", to: "quality", kind: "artifact" },
    { id: "quality-save", from: "quality", to: "save", kind: "artifact" }
  ]
};

export const sampleWorkflows = [
  textToImageZhWorkflow,
  controlNetCannyWorkflow,
  inpaintWorkflow,
  loraStackWorkflow
];
