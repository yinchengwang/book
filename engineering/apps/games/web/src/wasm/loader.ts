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
      // Always use an absolute /wasm/ prefix so the browser resolves the URL
      // against the server root rather than the current SPA route.
      locateFile: (path) => `${opts.wasmPath ?? '/wasm'}/${path}`,
    });

    return module as unknown as GameExports;
  })();

  return modulePromise;
}

export function resetWasmCache() {
  modulePromise = null;
}