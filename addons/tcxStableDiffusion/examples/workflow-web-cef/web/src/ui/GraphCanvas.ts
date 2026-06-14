import type { TxcSdWorkflow, WorkflowEdge, WorkflowNode } from "../workflow/schema";
import { fieldLabel, nodeKindLabel, requestModeLabel } from "../workflow/labels";

export interface GraphCanvasEvents {
  onSelect(nodeId: string): void;
  onConnect(from: string, to: string): void;
  onMove(nodeId: string, x: number, y: number): void;
}

const edgeColor: Record<WorkflowEdge["kind"], string> = {
  prompt: "#e1c16a",
  negativePrompt: "#a99368",
  image: "#70d398",
  mask: "#f87171",
  control: "#c59a42",
  lora: "#d6a85f",
  settings: "#8b7b55",
  artifact: "#f4e6c1"
};

export class GraphCanvas {
  private selectedId = "";
  private connectingFrom = "";
  private dragging: { id: string; startX: number; startY: number; nodeX: number; nodeY: number } | null = null;

  constructor(private root: HTMLElement, private events: GraphCanvasEvents) {}

  render(workflow: TxcSdWorkflow, selectedId: string) {
    this.selectedId = selectedId;
    const bounds = { minX: 0, minY: 0, width: 920, height: 620 };
    this.root.innerHTML = `
      <svg class="graph-edges" viewBox="${bounds.minX} ${bounds.minY} ${bounds.width} ${bounds.height}" aria-hidden="true">
        ${workflow.edges.map((edge) => this.edgeSvg(edge, workflow.nodes)).join("")}
      </svg>
      <div class="graph-nodes">
        ${workflow.nodes.map((node) => this.nodeHtml(node)).join("")}
      </div>
    `;
    this.bindNodeEvents();
  }

  private nodeHtml(node: WorkflowNode) {
    const selected = node.id === this.selectedId ? " selected" : "";
    const summary = this.nodeSummary(node);
    return `
      <button class="graph-node${selected}" style="left:${node.x}px; top:${node.y}px" data-node-id="${node.id}">
        <span class="node-kind">${nodeKindLabel(node.kind)}</span>
        <strong>${escapeHtml(node.label)}</strong>
        <small>${escapeHtml(summary)}</small>
        <span class="node-ports">
          <span data-port="in"></span>
          <span data-port="out"></span>
        </span>
      </button>
    `;
  }

  private nodeSummary(node: WorkflowNode) {
    if (node.kind === "Prompt") return String(node.data.prompt || "").slice(0, 48);
    if (node.kind === "ModelProfile") return String(node.data.model || "");
    if (node.kind === "Generate") return requestModeLabel(node.data.requestMode || "text_to_image");
    if (node.kind === "ControlNet") return String(node.data.controlImage || "");
    if (node.kind === "LoRAStack") {
      const loras = Array.isArray(node.data.loras) ? node.data.loras : [];
      return `${loras.length} LoRA`;
    }
    return Object.keys(node.data).slice(0, 2).map(fieldLabel).join(", ");
  }

  private edgeSvg(edge: WorkflowEdge, nodes: WorkflowNode[]) {
    const from = nodes.find((node) => node.id === edge.from);
    const to = nodes.find((node) => node.id === edge.to);
    if (!from || !to) return "";
    const x1 = from.x + 220;
    const y1 = from.y + 48;
    const x2 = to.x;
    const y2 = to.y + 48;
    const mid = (x1 + x2) / 2;
    return `<path d="M ${x1} ${y1} C ${mid} ${y1}, ${mid} ${y2}, ${x2} ${y2}" stroke="${edgeColor[edge.kind]}" />`;
  }

  private bindNodeEvents() {
    this.root.querySelectorAll<HTMLButtonElement>(".graph-node").forEach((nodeEl) => {
      const nodeId = nodeEl.dataset.nodeId || "";
      nodeEl.addEventListener("pointerdown", (event) => {
        const target = event.target as HTMLElement;
        this.events.onSelect(nodeId);
        if (target.dataset.port === "out") {
          this.connectingFrom = nodeId;
          return;
        }
        if (target.dataset.port === "in" && this.connectingFrom && this.connectingFrom !== nodeId) {
          this.events.onConnect(this.connectingFrom, nodeId);
          this.connectingFrom = "";
          return;
        }
        this.dragging = {
          id: nodeId,
          startX: event.clientX,
          startY: event.clientY,
          nodeX: Number.parseFloat(nodeEl.style.left || "0"),
          nodeY: Number.parseFloat(nodeEl.style.top || "0")
        };
        nodeEl.setPointerCapture(event.pointerId);
      });
      nodeEl.addEventListener("pointermove", (event) => {
        if (!this.dragging || this.dragging.id !== nodeId) return;
        const x = Math.max(0, this.dragging.nodeX + event.clientX - this.dragging.startX);
        const y = Math.max(0, this.dragging.nodeY + event.clientY - this.dragging.startY);
        this.events.onMove(nodeId, x, y);
      });
      nodeEl.addEventListener("pointerup", () => {
        this.dragging = null;
      });
    });
  }
}

export function escapeHtml(value: unknown) {
  return String(value ?? "")
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}
