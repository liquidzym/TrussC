import type { BackendMessage } from "../workflow/schema";
import { detailLabel, errorMessageLabel, hintLabel, statusLabel } from "../workflow/labels";
import { escapeHtml } from "./GraphCanvas";

export interface QueueEntry {
  id: string;
  status: string;
  detail: string;
  error?: string;
  hints?: string[];
}

export interface QueueEvents {
  onCancel(id: string): void;
}

export class QueuePanel {
  constructor(private root: HTMLElement, private events: QueueEvents) {}

  render(entries: QueueEntry[]) {
    this.root.innerHTML = entries.length
      ? entries.map((entry) => this.row(entry)).join("")
      : `<div class="queue-empty">暂无任务</div>`;
    this.root.querySelectorAll<HTMLButtonElement>("[data-cancel]").forEach((button) => {
      button.addEventListener("click", () => this.events.onCancel(button.dataset.cancel || ""));
    });
  }

  reduce(entries: QueueEntry[], message: BackendMessage): QueueEntry[] {
    const id = message.id || message.jobId || "host";
    const next = [...entries];
    const index = next.findIndex((entry) => entry.id === id);
    const existing = index >= 0 ? next[index] : { id, status: "pending", detail: "" };
    const entry: QueueEntry = {
      ...existing,
      status: message.type,
      detail: message.error
        ? errorMessageLabel(message.error.code, message.error.message)
        : detailLabel(message.detail || message.stage || existing.detail),
      error: message.error?.message,
      hints: message.error?.remediation_hints
    };
    if (index >= 0) next[index] = entry;
    else next.unshift(entry);
    return next.slice(0, 8);
  }

  private row(entry: QueueEntry) {
    const hints = entry.hints?.length
      ? `<ul>${entry.hints.map((hint) => `<li>${escapeHtml(hintLabel(hint))}</li>`).join("")}</ul>`
      : "";
    return `
      <div class="queue-row ${entry.error ? "has-error" : ""}">
        <div>
          <strong>${escapeHtml(statusLabel(entry.status))}</strong>
          <span>${escapeHtml(entry.detail)}</span>
          ${hints}
        </div>
        <button class="icon-button" data-cancel="${escapeHtml(entry.id)}" title="取消">X</button>
      </div>
    `;
  }
}
