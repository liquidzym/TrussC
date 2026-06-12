import "./theme.css";
import { sampleWorkflows } from "./workflow/examples";
import type { BackendMessage, BridgeCommand, TxcSdWorkflow, WorkflowEdge, WorkflowNode } from "./workflow/schema";
import { validateWorkflow } from "./workflow/validate";
import { nodeKindLabel } from "./workflow/labels";
import { Gallery, type GalleryItem } from "./ui/Gallery";
import { GraphCanvas } from "./ui/GraphCanvas";
import { Inspector } from "./ui/Inspector";
import { QueuePanel, type QueueEntry } from "./ui/QueuePanel";

const app = document.querySelector<HTMLDivElement>("#app");
if (!app) throw new Error("Missing #app root");

let workflow = structuredClone(sampleWorkflows[0]);
let selectedId = workflow.nodes[0]?.id || "";
let connected = false;
let queueEntries: QueueEntry[] = [];
let galleryItems: GalleryItem[] = [];
let sidecarText = "";
let socket: WebSocket | null = null;

app.innerHTML = `
  <main class="app-shell">
    <header class="topbar">
      <span class="brand">tcxStableDiffusion 工作流工作台</span>
      <span id="connection-dot" class="status-dot"></span>
      <span id="connection-label">桥接未连接</span>
      <div class="toolbar">
        <button id="validate-button">检查</button>
        <button id="run-button">运行</button>
        <button id="list-loras-button">LoRA 列表</button>
      </div>
    </header>
    <aside class="palette">
      <h2>工作流</h2>
      <div id="sample-list" class="palette-list"></div>
      <h2 style="margin-top:18px">节点</h2>
      <div id="node-list" class="palette-list"></div>
    </aside>
    <section class="canvas-shell">
      <div id="graph-canvas" class="graph-canvas"></div>
    </section>
    <aside class="inspector">
      <h2>检查器</h2>
      <div id="inspector-root"></div>
    </aside>
    <section class="queue">
      <h2>队列</h2>
      <div id="queue-root"></div>
    </section>
    <section class="gallery">
      <h2>图库 / 侧车</h2>
      <div id="gallery-root"></div>
    </section>
  </main>
`;

const graph = new GraphCanvas(required("graph-canvas"), {
  onSelect(nodeId) {
    selectedId = nodeId;
    render();
  },
  onMove(nodeId, x, y) {
    workflow = patchNode(workflow, nodeId, {}, { x, y });
    render();
  },
  onConnect(from, to) {
    workflow = {
      ...workflow,
      edges: [...workflow.edges, inferEdge(workflow, from, to)]
    };
    render();
  }
});

const inspector = new Inspector(required("inspector-root"), {
  onPatch(nodeId, patch) {
    workflow = patchNode(workflow, nodeId, patch);
    render();
  }
});

const queue = new QueuePanel(required("queue-root"), {
  onCancel(id) {
    send({ id: crypto.randomUUID(), type: "cancelJob", jobId: id });
  }
});

const gallery = new Gallery(required("gallery-root"), {
  onOpenSidecar(path) {
    send({ id: crypto.randomUUID(), type: "openSidecar", payload: { path } });
  },
  onReuseSeed(seed) {
    const generate = workflow.nodes.find((node) => node.kind === "Generate");
    if (generate) workflow = patchNode(workflow, generate.id, { seed: Number(seed) });
    render();
  }
});

required("validate-button").addEventListener("click", () => {
  const validation = validateWorkflow(workflow);
  queueEntries = queue.reduce(queueEntries, {
    id: crypto.randomUUID(),
    type: validation.ok ? "validation-ok" : "validation-error",
    detail: validation.ok ? "工作流检查通过" : validation.errors.map((item) => item.message).join("; ")
  });
  render();
  send({ id: crypto.randomUUID(), type: "validateWorkflow", workflow });
});

required("run-button").addEventListener("click", () => {
  const validation = validateWorkflow(workflow);
  if (!validation.ok) {
    queueEntries = queue.reduce(queueEntries, {
      id: crypto.randomUUID(),
      type: "validation-error",
      detail: validation.errors.map((item) => item.message).join("; ")
    });
    render();
    return;
  }
  send({ id: crypto.randomUUID(), type: "runWorkflow", workflow });
});

required("list-loras-button").addEventListener("click", () => {
  send({ id: crypto.randomUUID(), type: "listLoras", payload: {} });
});

function connectBridge() {
  const port = new URLSearchParams(location.search).get("bridgePort");
  if (!port) {
    updateConnection(false, "请从桌面宿主程序打开");
    return;
  }
  socket = new WebSocket(`ws://127.0.0.1:${port}/bridge`);
  socket.addEventListener("open", () => updateConnection(true, `桥接端口 ${port}`));
  socket.addEventListener("close", () => updateConnection(false, "桥接已关闭"));
  socket.addEventListener("error", () => updateConnection(false, "桥接连接异常"));
  socket.addEventListener("message", (event) => {
    handleBackendMessage(JSON.parse(String(event.data)) as BackendMessage);
  });
}

function send(command: BridgeCommand) {
  if (!socket || socket.readyState !== WebSocket.OPEN) {
    queueEntries = queue.reduce(queueEntries, {
      id: command.id,
      type: "bridge-offline",
      detail: "本地桥接尚未连接"
    });
    render();
    return;
  }
  socket.send(JSON.stringify(command));
}

function handleBackendMessage(message: BackendMessage) {
  queueEntries = queue.reduce(queueEntries, message);
  galleryItems = gallery.reduce(galleryItems, message);
  if (message.type === "sidecar" && message.result?.text) {
    sidecarText = String(message.result.text);
  }
  if (message.type === "listLoras" && message.loras) {
    sidecarText = JSON.stringify(message.loras, null, 2);
  }
  render();
}

function render() {
  renderSamples();
  renderPalette();
  graph.render(workflow, selectedId);
  inspector.render(workflow.nodes.find((node) => node.id === selectedId));
  queue.render(queueEntries);
  gallery.render(galleryItems, sidecarText);
}

function renderSamples() {
  const sampleList = required("sample-list");
  sampleList.innerHTML = sampleWorkflows.map((item, index) => `
    <button class="sample-button ${item.id === workflow.id ? "active" : ""}" data-sample="${index}">
      ${item.title}
    </button>
  `).join("");
  sampleList.querySelectorAll<HTMLButtonElement>("[data-sample]").forEach((button) => {
    button.addEventListener("click", () => {
      workflow = structuredClone(sampleWorkflows[Number(button.dataset.sample || 0)]);
      selectedId = workflow.nodes[0]?.id || "";
      render();
    });
  });
}

function renderPalette() {
  const nodeList = required("node-list");
  const kinds: WorkflowNode["kind"][] = ["ModelProfile", "RuntimePreset", "Prompt", "SourceImage", "ControlNet", "LoRAStack", "Generate", "QualityCheck", "SaveArtifact"];
  nodeList.innerHTML = kinds.map((kind) => `<button class="palette-button" data-kind="${kind}">${nodeKindLabel(kind)}</button>`).join("");
  nodeList.querySelectorAll<HTMLButtonElement>("[data-kind]").forEach((button) => {
    button.addEventListener("click", () => {
      const kind = button.dataset.kind as WorkflowNode["kind"];
      const node: WorkflowNode = {
        id: `${kind.toLowerCase()}-${Date.now()}`,
        kind,
        label: nodeKindLabel(kind),
        x: 140 + workflow.nodes.length * 22,
        y: 120 + workflow.nodes.length * 18,
        data: defaultNodeData(kind)
      };
      workflow = { ...workflow, nodes: [...workflow.nodes, node] };
      selectedId = node.id;
      render();
    });
  });
}

function patchNode(workflowValue: TxcSdWorkflow, nodeId: string, dataPatch: Record<string, unknown>, position?: { x: number; y: number }): TxcSdWorkflow {
  return {
    ...workflowValue,
    nodes: workflowValue.nodes.map((node) => node.id === nodeId
      ? { ...node, ...position, data: { ...node.data, ...dataPatch } }
      : node)
  };
}

function inferEdge(workflowValue: TxcSdWorkflow, from: string, to: string): WorkflowEdge {
  const source = workflowValue.nodes.find((node) => node.id === from);
  const target = workflowValue.nodes.find((node) => node.id === to);
  const kind: WorkflowEdge["kind"] =
    source?.kind === "Prompt" ? "prompt" :
    source?.kind === "NegativePrompt" ? "negativePrompt" :
    source?.kind === "SourceImage" ? "image" :
    source?.kind === "MaskImage" ? "mask" :
    source?.kind === "ControlNet" ? "control" :
    source?.kind === "LoRAStack" ? "lora" :
    target?.kind === "SaveArtifact" ? "artifact" :
    "settings";
  return { id: `${from}-${to}-${Date.now()}`, from, to, kind };
}

function defaultNodeData(kind: WorkflowNode["kind"]): Record<string, unknown> {
  switch (kind) {
    case "ModelProfile": return { model: "flux2-klein-4b-q4_0" };
    case "RuntimePreset": return { runtimePreset: "lowVram", quality: "draft", width: 512, height: 512, steps: 6 };
    case "Prompt": return { prompt: "一张精致的本地 AI 图像生成工作流结果。" };
    case "NegativePrompt": return { negativePrompt: "低质量，模糊，水印，签名" };
    case "SourceImage": return { path: "bin/data/inputs/source.png" };
    case "MaskImage": return { path: "bin/data/inputs/mask.png" };
    case "ControlNet": return { controlImage: "bin/data/inputs/control/canny-guide.png", controlStrength: 1.0 };
    case "LoRAStack": return { loras: [{ path: "starter/style.safetensors", weight: 0.65 }] };
    case "Generate": return { requestMode: "text_to_image", outputName: "generated" };
    case "QualityCheck": return { rejectBlank: true, rejectWrongSize: true };
    case "SaveArtifact": return { projectName: "workflow-web-cef" };
    case "BatchSeeds": return { seeds: [11, 12, 13] };
  }
}

function required(id: string) {
  const element = document.getElementById(id);
  if (!element) throw new Error(`Missing #${id}`);
  return element;
}

function updateConnection(isConnected: boolean, label: string) {
  connected = isConnected;
  const dot = required("connection-dot");
  dot.classList.toggle("connected", connected);
  required("connection-label").textContent = label;
}

connectBridge();
render();
