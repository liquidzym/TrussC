export class Bridge {
  private socket: WebSocket | null = null;
  private queue: string[] = [];

  onConfig: ((message: unknown) => void) | null = null;

  connect(port: number): void {
    this.socket = new WebSocket(`ws://127.0.0.1:${port}/bridge`);
    this.socket.addEventListener("open", () => {
      const queued = this.queue.splice(0);
      for (const message of queued) {
        this.socket?.send(message);
      }
    });
    this.socket.addEventListener("message", (event) => {
      try {
        const message = JSON.parse(String(event.data));
        if (message.type === "config") {
          this.onConfig?.(message);
        }
      } catch {
        this.send({ type: "runtime_status", ready: false, reason: "Invalid config JSON" });
      }
    });
  }

  send(value: unknown): void {
    const text = JSON.stringify(value);
    if (this.socket?.readyState === WebSocket.OPEN) {
      this.socket.send(text);
    } else {
      this.queue.push(text);
    }
  }
}
