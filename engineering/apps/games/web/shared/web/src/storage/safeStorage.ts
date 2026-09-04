// shared/web/src/storage/safeStorage.ts
/**
 * Safe localStorage helpers that swallow errors from disabled storage,
 * quota exceeded, or invalid JSON. Designed to be safe in SSR / private mode.
 */

export function safeGet<T>(key: string, fallback: T): T {
  try {
    const raw = localStorage.getItem(key);
    if (raw === null) return fallback;
    return JSON.parse(raw) as T;
  } catch {
    return fallback;
  }
}

export function safeSet<T>(key: string, value: T): void {
  try {
    localStorage.setItem(key, JSON.stringify(value));
  } catch {
    // quota exceeded / storage disabled / private mode
  }
}

export function safeGetNumber(key: string, fallback: number): number {
  const v = safeGet<unknown>(key, fallback);
  return typeof v === 'number' && Number.isFinite(v) ? v : fallback;
}
