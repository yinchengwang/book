// src/games/g2048/renderer.ts
export function renderBoard(
  ctx: CanvasRenderingContext2D,
  tiles: number[][],
  cellSize = 100,
  padding = 10
) {
  const size = tiles.length;
  // 背景
  ctx.fillStyle = '#bbada0';
  ctx.fillRect(0, 0, size * cellSize, size * cellSize);

  for (let r = 0; r < size; r++) {
    for (let c = 0; c < size; c++) {
      const v = tiles[r][c];
      const x = c * cellSize + padding;
      const y = r * cellSize + padding;
      const sz = cellSize - padding * 2;
      ctx.fillStyle = v === 0 ? '#cdc1b4' : tileColor(v);
      roundRect(ctx, x, y, sz, sz, 6);
      ctx.fill();

      if (v !== 0) {
        ctx.fillStyle = v <= 4 ? '#776e65' : '#f9f6f2';
        ctx.font = `bold ${sz * 0.45}px Arial`;
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillText(String(v), x + sz / 2, y + sz / 2);
      }
    }
  }
}

function tileColor(v: number): string {
  const map: Record<number, string> = {
    2: '#eee4da', 4: '#ede0c8', 8: '#f2b179', 16: '#f59563',
    32: '#f67c5f', 64: '#f65e3b', 128: '#edcf72', 256: '#edcc61',
    512: '#edc850', 1024: '#edc53f', 2048: '#edc22e',
  };
  return map[v] || '#3c3a32';
}

function roundRect(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, r: number) {
  ctx.beginPath();
  ctx.moveTo(x + r, y);
  ctx.lineTo(x + w - r, y);
  ctx.quadraticCurveTo(x + w, y, x + w, y + r);
  ctx.lineTo(x + w, y + h - r);
  ctx.quadraticCurveTo(x + w, y + h, x + w - r, y + h);
  ctx.lineTo(x + r, y + h);
  ctx.quadraticCurveTo(x, y + h, x, y + h - r);
  ctx.lineTo(x, y + r);
  ctx.quadraticCurveTo(x, y, x + r, y);
  ctx.closePath();
}