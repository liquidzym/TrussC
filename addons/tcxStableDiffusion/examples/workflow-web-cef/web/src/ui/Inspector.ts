import type { WorkflowNode } from "../workflow/schema";
import { fieldLabel, nodeKindLabel } from "../workflow/labels";
import { escapeHtml } from "./GraphCanvas";

export interface InspectorEvents {
  onPatch(nodeId: string, patch: Record<string, unknown>): void;
}

export class Inspector {
  constructor(private root: HTMLElement, private events: InspectorEvents) {}

  render(node?: WorkflowNode) {
    if (!node) {
      this.root.innerHTML = `<div class="empty-state">请选择一个节点</div>`;
      return;
    }
    const rows = Object.entries(node.data)
      .map(([key, value]) => this.field(node.id, key, value))
      .join("");
    this.root.innerHTML = `
      <header class="panel-header">
        <span>${escapeHtml(nodeKindLabel(node.kind))}</span>
        <strong>${escapeHtml(node.label)}</strong>
      </header>
      <div class="field-list">${rows || `<div class="empty-state">没有可编辑字段</div>`}</div>
    `;
    this.bind();
  }

  private field(nodeId: string, key: string, value: unknown) {
    const serialized = typeof value === "object" ? JSON.stringify(value, null, 2) : String(value ?? "");
    const multiline = serialized.length > 64 || serialized.includes("\n");
    if (multiline) {
      return `
        <label class="field">
          <span>${escapeHtml(fieldLabel(key))}</span>
          <textarea data-node-id="${nodeId}" data-key="${key}">${escapeHtml(serialized)}</textarea>
        </label>
      `;
    }
    return `
      <label class="field">
        <span>${escapeHtml(fieldLabel(key))}</span>
        <input data-node-id="${nodeId}" data-key="${key}" value="${escapeHtml(serialized)}" />
      </label>
    `;
  }

  private bind() {
    this.root.querySelectorAll<HTMLInputElement | HTMLTextAreaElement>("[data-key]").forEach((field) => {
      field.addEventListener("change", () => {
        const nodeId = field.dataset.nodeId || "";
        const key = field.dataset.key || "";
        this.events.onPatch(nodeId, { [key]: parseValue(field.value) });
      });
    });
  }
}

function parseValue(value: string): unknown {
  const trimmed = value.trim();
  if (!trimmed) return "";
  if (trimmed === "true") return true;
  if (trimmed === "false") return false;
  if (/^-?\d+(\.\d+)?$/.test(trimmed)) return Number(trimmed);
  if ((trimmed.startsWith("{") && trimmed.endsWith("}")) || (trimmed.startsWith("[") && trimmed.endsWith("]"))) {
    try {
      return JSON.parse(trimmed);
    } catch {
      return value;
    }
  }
  return value;
}
