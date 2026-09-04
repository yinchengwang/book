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
 * Eagerly raw-load every learn-deep markdown file at build/dev time.
 * Key format: absolute-ish path under /data/learn-deep/.../<cat>-<itemId>.md.
 */
const rawLearnFiles = import.meta.glob<string>(
  '@data/learn-deep/**/*.md',
  { query: '?raw', import: 'default' }
);

const FALLBACK_CONTENT =
  '# 内容加载失败\n\n该知识点暂无详细内容。';

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
 * Returns a friendly fallback when the file is missing so the UI never
 * crashes on a missing entry.
 */
export async function loadLearnContent(
  cat: TechCategory,
  quadrant: Quadrant,
  itemId: string
): Promise<string> {
  const entry = learnIndex.find(
    (e) => e.cat === cat && e.quadrant === quadrant && e.itemId === itemId
  );
  if (!entry) return FALLBACK_CONTENT;
  try {
    return await entry.load();
  } catch {
    return FALLBACK_CONTENT;
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