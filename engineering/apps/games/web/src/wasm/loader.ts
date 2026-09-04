// src/wasm/loader.ts
import type { GameExports } from './types';

let modulePromise: Promise<GameExports> | null = null;

export interface LoadOptions {
  wasmPath?: string;
  onProgress?: (loaded: number, total: number) => void;
}

export async function loadWasm(opts: LoadOptions = {}): Promise<GameExports> {
  if (modulePromise) return modulePromise;

  modulePromise = (async () => {
    if (typeof window === 'undefined' || !window.GameModule) {
      throw new Error(
        '[wasm] GameModule 未找到。请确认 public/wasm/games.js 已生成。' +
        '运行: npm run wasm:build'
      );
    }

    const module = await window.GameModule({
      locateFile: (path) => opts.wasmPath ? `${opts.wasmPath}/${path}` : path,
    });

    return module as unknown as GameExports;
  })();

  return modulePromise;
}

export function resetWasmCache() {
  modulePromise = null;
}