import type { BackendMessage } from "../workflow/schema";
import { escapeHtml } from "./GraphCanvas";

export interface GalleryItem {
  outputPath: string;
  sidecarPath: string;
  workflowId: string;
  seed?: string;
}

export interface GalleryEvents {
  onOpenSidecar(path: string): void;
  onReuseSeed(seed: string): void;
}

export class Gallery {
  constructor(private root: HTMLElement, private events: GalleryEvents) {}

  render(items: GalleryItem[], sidecarText = "") {
    this.root.innerHTML = `
      <div class="gallery-strip">
        ${items.length ? items.map((item) => this.tile(item)).join("") : `<div class="gallery-empty">暂无输出</div>`}
      </div>
      <pre class="sidecar-preview">${escapeHtml(sidecarText || "侧车 JSON 会显示在这里")}</pre>
    `;
    this.root.querySelectorAll<HTMLButtonElement>("[data-sidecar]").forEach((button) => {
      button.addEventListener("click", () => this.events.onOpenSidecar(button.dataset.sidecar || ""));
    });
    this.root.querySelectorAll<HTMLButtonElement>("[data-seed]").forEach((button) => {
      button.addEventListener("click", () => this.events.onReuseSeed(button.dataset.seed || ""));
    });
  }

  reduce(items: GalleryItem[], message: BackendMessage): GalleryItem[] {
    if (message.type !== "result" || !message.result) return items;
    const result = message.result;
    const outputPath = String(result.outputPath || "");
    const sidecarPath = String(result.sidecarPath || "");
    if (!outputPath) return items;
    return [{
      outputPath,
      sidecarPath,
      workflowId: String(result.workflowId || ""),
      seed: result.seed === undefined ? undefined : String(result.seed)
    }, ...items].slice(0, 16);
  }

  private tile(item: GalleryItem) {
    return `
      <article class="gallery-tile">
        <div class="thumb">${escapeHtml(item.outputPath.split(/[\\/]/).pop() || "PNG")}</div>
        <strong>${escapeHtml(item.workflowId || "工作流")}</strong>
        <button data-sidecar="${escapeHtml(item.sidecarPath)}">侧车</button>
        ${item.seed ? `<button data-seed="${escapeHtml(item.seed)}">复用种子 ${escapeHtml(item.seed)}</button>` : ""}
      </article>
    `;
  }
}
