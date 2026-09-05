// Learn-deep Markdown loader.
//
// Source layout (matches the legacy learn.html):
//   data/learn-deep/<cat>/<quadrant>/<cat>-<itemId>.md
//   e.g. data/learn-deep/c/language/c-syntax.md
//
// Note: file names are prefixed with the category (cat-itemId.md), not
// just `<itemId>.md`. The `cat-` prefix is easy to miss when reading a
// spec — verify against disk before changing.

import type { TechCategory, Quadrant } from './types';

/**
 * Result of loading a learn-deep markdown file.
 * Distinguishes "file missing from index" from "load threw", so the
 * Learn page can render an accurate error message instead of silently
 * showing fallback content as if it were real.
 */
export type LearnContentResult =
  | { ok: true; content: string }
  | { ok: false; reason: 'not-found' | 'load-error' };

/**
 * Eagerly raw-load every learn-deep markdown file at build/dev time.
 * Key format: absolute-ish path under /data/learn-deep/.../<cat>-<itemId>.md.
 */
const rawLearnFiles = import.meta.glob<string>(
  '@data/learn-deep/**/*.md',
  { query: '?raw', import: 'default' }
);

const PATH_RE = /\/learn-deep\/([^/]+)\/([^/]+)\/([a-z]+)-([^/]+)\.md$/;

/**
 * Pre-index all markdown paths for O(1) lookups by (cat, quadrant, itemId).
 * `quadrant` is taken from the directory name (legacy "language"/"systems"/...).
 */
interface IndexedEntry {
  cat: TechCategory;
  quadrant: Quadrant;
  itemId: string;
  load: () => Promise<string>;
}

const learnIndex: IndexedEntry[] = (() => {
  const list: IndexedEntry[] = [];
  for (const [path, load] of Object.entries(rawLearnFiles)) {
    const m = PATH_RE.exec(path);
    if (!m) continue;
    list.push({
      cat: m[1] as TechCategory,
      quadrant: m[2] as Quadrant,
      itemId: m[4],
      load,
    });
  }
  return list;
})();

/**
 * Load a single learn-deep markdown file as text.
 * Returns a result object so callers can distinguish "no such file"
 * ('not-found') from "file exists but load threw" ('load-error').
 * Never throws — errors are folded into the result.
 *
 * itemId comes from the tech registry, which uses snake_case ids
 * (`control_flow`), while learn-deep filenames are kebab-case
 * (`c-control-flow.md` → indexed as `control-flow`). Both forms are
 * tried so registry-sourced URLs resolve.
 */
export async function loadLearnContent(
  cat: TechCategory,
  quadrant: Quadrant,
  itemId: string
): Promise<LearnContentResult> {
  const candidates = new Set([
    itemId,
    itemId.replace(/_/g, '-'),
    itemId.replace(/-/g, '_'),
  ]);
  const entry = learnIndex.find(
    (e) => e.cat === cat && e.quadrant === quadrant && candidates.has(e.itemId)
  );
  if (!entry) return { ok: false, reason: 'not-found' };
  try {
    const content = await entry.load();
    return { ok: true, content };
  } catch (err) {
    // eslint-disable-next-line no-console
    console.error('[data/learn] load failed:', cat, quadrant, itemId, err);
    return { ok: false, reason: 'load-error' };
  }
}

/**
 * List itemIds that have markdown content for a given (cat, quadrant).
 * Useful when building the learn view's TOC.
 */
export function listLearnItems(
  cat: TechCategory,
  quadrant: Quadrant
): string[] {
  return learnIndex
    .filter((e) => e.cat === cat && e.quadrant === quadrant)
    .map((e) => e.itemId)
    .sort();
}