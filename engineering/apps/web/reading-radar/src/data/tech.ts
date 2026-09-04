// Tech items loader.
//
// Source: data/app/items-registry.js
//   The registry is defined as a top-level `const ITEMS_REGISTRY = { ... }` inside
//   a "use strict" script — it has no `export` statements. Importing it directly
//   from ESM is not possible, so we read the file as raw text via Vite's `?raw`
//   query, then evaluate the source inside a sandboxed Function. The ITEMS_REGISTRY
//   binding never leaks to the global scope.

// Cast the raw import as `string` so the rest of the file can stay
// strictly typed. Vite's `?raw` returns string at runtime; the ambient
// module declaration in vite-env.d.ts handles the generic case but
// TypeScript still widens to unknown without an explicit cast.
import itemsRegistrySource from '@data/app/items-registry.js?raw';

const _itemsRegistrySource: string = itemsRegistrySource;

import type { TechCategory, TechItem } from './types';

interface RawRegistryEntry {
  stack: TechCategory;
  title: string;
  quadrant: TechItem['quadrant'];
  ring: TechItem['ring'];
  desc: string;
  tags?: string[];
  /** Present in radar-tech derivatives (product: "network" etc.) — ignored. */
  product?: string;
}

type RawRegistry = Record<string, RawRegistryEntry>;

let cachedRegistry: RawRegistry | null = null;

/**
 * Lazily evaluate the items-registry source and return the ITEMS_REGISTRY map.
 * The result is memoized so multiple loaders share one parse.
 *
 * Wrapped in try/catch because items-registry.js is a 134KB hand-edited file:
 * a single syntax error would otherwise take down the whole module init.
 * Returning `{}` keeps loadTechItems / loadAllTechItems as empty-result
 * callers instead of throwing, so the UI can still render with an empty radar.
 */
function getRegistry(): RawRegistry {
  if (cachedRegistry) return cachedRegistry;

  try {
    // Append a `return ITEMS_REGISTRY;` to the source so the Function body exposes it.
    // The source already declares `const ITEMS_REGISTRY = { ... }` at the top level,
    // which is scoped to the Function body and not the global scope.
    const body = `${_itemsRegistrySource}\nreturn ITEMS_REGISTRY;`;
    // eslint-disable-next-line @typescript-eslint/no-implied-eval
    const fn = new Function(body);
    cachedRegistry = fn() as RawRegistry;
    return cachedRegistry;
  } catch (err) {
    // eslint-disable-next-line no-console
    console.error('[data/tech] Failed to load items-registry.js:', err);
    // Cache the empty fallback so we don't re-parse + log on every call.
    cachedRegistry = {};
    return cachedRegistry;
  }
}

/**
 * Reset the internal cache. Test-only escape hatch — keeps prod surface small.
 */
export function __resetTechCache(): void {
  cachedRegistry = null;
}

/**
 * Load every tech item belonging to the given stack/category.
 *
 * Async to keep the API consistent with other data loaders; the work itself
 * is synchronous (cached after first call).
 */
export async function loadTechItems(stack: TechCategory): Promise<TechItem[]> {
  const registry = getRegistry();
  const items: TechItem[] = [];
  for (const [id, entry] of Object.entries(registry)) {
    if (entry.stack !== stack) continue;
    items.push({
      id,
      title: entry.title,
      quadrant: entry.quadrant,
      ring: entry.ring,
      desc: entry.desc,
      tags: entry.tags,
    });
  }
  return items;
}

/**
 * Load every tech item across all stacks, keyed by stack name.
 * Useful for building the full radar at once.
 */
export async function loadAllTechItems(): Promise<Record<TechCategory, TechItem[]>> {
  const registry = getRegistry();
  const result: Record<string, TechItem[]> = {
    c: [],
    cpp: [],
    ds: [],
    db: [],
    py: [],
    linux: [],
    vdb: [],
    grok: [],
  };
  for (const [id, entry] of Object.entries(registry)) {
    const stack = entry.stack;
    if (!result[stack]) continue;
    result[stack].push({
      id,
      title: entry.title,
      quadrant: entry.quadrant,
      ring: entry.ring,
      desc: entry.desc,
      tags: entry.tags,
    });
  }
  return result as Record<TechCategory, TechItem[]>;
}

/**
 * Spec-named alias for `loadTechItems`. The original spec writes
 * `loadTechMeta(cat)`; the underlying implementation was renamed to
 * `loadTechItems(stack)` during refactor. This alias preserves both
 * names so spec code and existing callers keep working.
 *
 * @deprecated Prefer `loadTechItems` — kept for spec compliance.
 */
export const loadTechMeta = loadTechItems;