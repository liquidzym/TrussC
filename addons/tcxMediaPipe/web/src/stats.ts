export class RateCounter {
  private count = 0;
  private lastMs = performance.now();
  value = 0;

  tick(nowMs = performance.now()): number {
    this.count += 1;
    const elapsed = nowMs - this.lastMs;
    if (elapsed >= 1000) {
      this.value = (this.count * 1000) / elapsed;
      this.count = 0;
      this.lastMs = nowMs;
    }
    return this.value;
  }
}
