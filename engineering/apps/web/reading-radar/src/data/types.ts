// Shared type definitions for the reading-radar data layer.
// Keep this file dependency-free so it can be imported anywhere.

export type TechCategory =
  | 'c'
  | 'cpp'
  | 'ds'
  | 'db'
  | 'py'
  | 'linux'
  | 'vdb'
  | 'grok';

export type Quadrant =
  | 'language'
  | 'systems'
  | 'algorithms'
  | 'engineering';

export type Ring = 'basic' | 'intermediate' | 'advanced';

/**
 * A single knowledge point shown on the radar / tech-tree UI.
 * Derived from `data/app/items-registry.js`.
 */
export interface TechItem {
  id: string;
  title: string;
  quadrant: Quadrant;
  ring: Ring;
  desc: string;
  /** Optional tags (only present in some registry entries). */
  tags?: string[];
}

/**
 * A single quiz question. The question bank files already self-describe
 * quadrant / ring on every entry, so they stay attached to the record.
 */
export interface Question {
  id: string;
  type: string;
  difficulty: string;
  scenario: string;
  stem: string;
  code?: string;
  /** Optional: true_false / fill_blank 等题型在真实题库中没有 options 字段. */
  options?: string[];
  answer: string | string[] | boolean;
  explanation: string;
  quadrant?: Quadrant;
  ring?: Ring;
}

/**
 * Result of loading a single category's question bank.
 * Keys are itemIds (e.g. "syntax", "file_io"); each value is the
 * question list for that item.
 */
export type QuestionBankByItem = Record<string, Question[]>;

/**
 * Full bank: category -> itemId -> questions.
 */
export type QuestionBank = Record<TechCategory, QuestionBankByItem>;