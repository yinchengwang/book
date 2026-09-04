// Question bank loader.
//
// Source layout (matches the legacy quiz-system.html):
//   data/quiz/questions/<cat>/quiz-questions-<cat>-<section>.js
//   e.g. data/quiz/questions/c/quiz-questions-c-language.js
//
// Each file does:
//   QUESTION_BANK.<cat> = Object.assign(QUESTION_BANK.<cat> || {}, { itemId: [...questions] })
//
//   So QUESTION_BANK must exist as a writable map before evaluation, and
//   the keys under each category are itemIds (e.g. "syntax", "file_io"),
//   not quadrant names. Each question object already carries its own
//   `quadrant` and `ring` fields.
//
//   The question files have no `export` statements, so we load them as raw
//   text via Vite's `?raw` query and evaluate them inside a sandboxed
//   Function with a shared QUESTION_BANK binding.

import type { Question, QuestionBankByItem, TechCategory } from './types';

/**
 * All question bank files, loaded eagerly as raw source strings.
 * Key format: absolute-ish path under /data/quiz/questions/.../file.js.
 * Vite resolves `@data/...` via the tsconfig + vite.config.ts alias.
 */
const rawQuestionFiles = import.meta.glob<string>(
  '@data/quiz/questions/*/quiz-questions-*-*.js',
  { query: '?raw', import: 'default' }
);

/** Categories available on disk (no `grok` in question banks). */
export const AVAILABLE_CATEGORIES = [
  'c',
  'cpp',
  'ds',
  'db',
  'py',
  'linux',
  'vdb',
] as const satisfies readonly TechCategory[];

interface FileEntry {
  section: string;
  load: () => Promise<string>;
}

/**
 * Index raw sources by category once at module init.
 * `regex` strips path and extracts the section name between
 * `quiz-questions-<cat>-` and `.js`.
 */
function indexByCategory(): Record<string, FileEntry[]> {
  const map: Record<string, FileEntry[]> = {};
  const re = /\/quiz-questions-([a-z]+)-(.+?)\.js$/;
  for (const [path, load] of Object.entries(rawQuestionFiles)) {
    const m = re.exec(path);
    if (!m) continue;
    const [, cat, section] = m;
    if (!map[cat]) map[cat] = [];
    map[cat].push({ section, load });
  }
  return map;
}

const fileIndex = indexByCategory();

/**
 * Memoized question banks keyed by category.
 *
 * `loadQuestionsByCat` re-parses every `?raw` source and re-runs
 * `new Function` on each call — expensive when the radar UI asks for
 * the same category's bank dozens of times. The cache lets the second
 * and later calls return the merged bank in O(1).
 *
 * Cache lives at module scope and never expires (the underlying source
 * files are static and shipped with the build).
 */
const questionCache = new Map<TechCategory, QuestionBankByItem>();

/**
 * Execute a single question-file source against a shared QUESTION_BANK map
 * and return the merged entry for `cat`.
 */
async function evalFile(
  source: string,
  cat: string,
  bank: Record<string, Record<string, Question[]>>
): Promise<void> {
  const fn = new Function('QUESTION_BANK', `${source}\n;`);
  fn(bank);
  // Sanity: warn if the file didn't touch the expected category.
  if (!bank[cat]) {
    // eslint-disable-next-line no-console
    console.warn(`[data/questions] file did not populate QUESTION_BANK.${cat}`);
  }
}

/**
 * Load every question for a category, grouped by itemId.
 * Returns an empty record when the category has no files on disk.
 *
 * Result is memoized in `questionCache` so repeated calls (one per item
 * in the radar UI, etc.) avoid re-parsing every `?raw` source and
 * re-running `new Function` for every section.
 */
export async function loadQuestionsByCat(
  cat: TechCategory
): Promise<QuestionBankByItem> {
  const cached = questionCache.get(cat);
  if (cached) return cached;

  const entries = fileIndex[cat];
  if (!entries || entries.length === 0) {
    questionCache.set(cat, {});
    return {};
  }
  const bank: Record<string, Record<string, Question[]>> = {};
  for (const entry of entries) {
    const source = await entry.load();
    evalFile(source, cat, bank);
  }
  const merged: QuestionBankByItem = bank[cat] ?? {};
  questionCache.set(cat, merged);
  return merged;
}

/**
 * Load a single section's contribution to a category.
 * The "section" is the segment between `quiz-questions-<cat>-` and `.js`,
 * e.g. "language", "mysql", "os-internals". Returns just the questions
 * for that section (still keyed by itemId).
 */
export async function loadQuestionsBySection(
  cat: TechCategory,
  section: string
): Promise<QuestionBankByItem> {
  const entries = fileIndex[cat] ?? [];
  const entry = entries.find((e) => e.section === section);
  if (!entry) return {};
  const source = await entry.load();
  const bank: Record<string, Record<string, Question[]>> = {};
  evalFile(source, cat, bank);
  return bank[cat] ?? {};
}

/**
 * Flatten a category bank into a single question array.
 * Convenience for quiz UIs that don't care about per-item grouping.
 */
export async function loadQuestionsFlat(cat: TechCategory): Promise<Question[]> {
  const grouped = await loadQuestionsByCat(cat);
  const out: Question[] = [];
  for (const list of Object.values(grouped)) {
    if (Array.isArray(list)) out.push(...list);
  }
  return out;
}

/**
 * List the section names available for a category. Useful for building
 * tabs / selectors without hardcoding "language/systems/...".
 */
export function listSections(cat: TechCategory): string[] {
  return (fileIndex[cat] ?? []).map((e) => e.section).sort();
}

/**
 * Spec-named alias: load the question list for a single (category, itemId)
 * pair. The original spec writes `loadQuestions(cat, itemId)`; the
 * implementation was split into `loadQuestionsByCat` / `loadQuestionsFlat`
 * to support both per-item and flat consumers. This wrapper composes them.
 *
 * Returns an empty array when the item has no questions on disk.
 */
export async function loadQuestions(
  cat: TechCategory,
  itemId: string
): Promise<Question[]> {
  const bank = await loadQuestionsByCat(cat);
  return bank[itemId] ?? [];
}