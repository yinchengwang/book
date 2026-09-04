// src/games/g2048/history.ts
export interface Snapshot {
  tiles: number[][]; // 4x4
  score: number;
}

export class History {
  private stack: Snapshot[] = [];
  constructor(private max = 20) {}

  push(s: Snapshot): void {
    this.stack.push(s);
    if (this.stack.length > this.max) this.stack.shift();
  }

  pop(): Snapshot | undefined {
    return this.stack.pop();
  }

  peek(): Snapshot | undefined {
    return this.stack[this.stack.length - 1];
  }

  clear(): void {
    this.stack = [];
  }

  get size(): number {
    return this.stack.length;
  }

  get canUndo(): boolean {
    return this.stack.length > 0;
  }
}
