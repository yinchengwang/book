// 数据层单元测试：questions.ts（题库加载器）
//
// 跑的是仓库内真实题库（`?raw` + import.meta.glob）。断言只用
// 结构性检查（> 0 / 字段存在 / 排序关系），不写死题目数量。

import { describe, expect, it } from 'vitest';
import {
  AVAILABLE_CATEGORIES,
  listSections,
  loadQuestions,
  loadQuestionsByCat,
  loadQuestionsBySection,
  loadQuestionsFlat,
} from '@/data/questions';
import type { Question, TechCategory } from '@/data/types';

function assertQuestionShape(q: Question): void {
  expect(typeof q.id).toBe('string');
  expect(typeof q.stem).toBe('string');
  // 真实题库中 fill_blank / true_false 题没有 options 字段（~25%），
  // 存在时必须是数组
  if (q.options !== undefined) {
    expect(Array.isArray(q.options)).toBe(true);
  }
  expect(q.answer).toBeDefined();
}

describe('questions / AVAILABLE_CATEGORIES', () => {
  it('等于磁盘上实际存在题库的 7 个分类（无 grok）', () => {
    expect([...AVAILABLE_CATEGORIES].sort()).toEqual(
      ['c', 'cpp', 'db', 'ds', 'linux', 'py', 'vdb']
    );
  });
});

describe('questions / loadQuestionsByCat', () => {
  it('每个可用分类都能加载出按 itemId 分组的题库', async () => {
    for (const cat of AVAILABLE_CATEGORIES) {
      const bank = await loadQuestionsByCat(cat);
      const itemIds = Object.keys(bank);
      expect(itemIds.length, `分类 ${cat} 应有题目`).toBeGreaterThan(0);
      for (const itemId of itemIds) {
        const list = bank[itemId];
        expect(Array.isArray(list), `${cat}/${itemId} 应为数组`).toBe(true);
        expect(list.length, `${cat}/${itemId} 应非空`).toBeGreaterThan(0);
        for (const q of list) assertQuestionShape(q);
      }
    }
  });

  it('重复调用返回同一引用（模块级缓存）', async () => {
    const first = await loadQuestionsByCat('c');
    const second = await loadQuestionsByCat('c');
    expect(second).toBe(first);
  });

  it('磁盘上无题库的分类（grok）返回空对象', async () => {
    const bank = await loadQuestionsByCat('grok');
    expect(bank).toEqual({});
  });
});

describe('questions / loadQuestionsBySection', () => {
  it('按 section 加载并仍以 itemId 分组', async () => {
    const bank = await loadQuestionsBySection('c', 'language');
    const itemIds = Object.keys(bank);
    expect(itemIds.length).toBeGreaterThan(0);
    for (const list of Object.values(bank)) {
      expect(list.length).toBeGreaterThan(0);
      for (const q of list) assertQuestionShape(q);
    }
  });

  it('不存在的 section 返回空对象', async () => {
    expect(await loadQuestionsBySection('c', '__nonexistent__')).toEqual({});
  });
});

describe('questions / listSections', () => {
  it('返回该分类磁盘上的 section 名且已排序', () => {
    const sections = listSections('c');
    expect(sections.length).toBeGreaterThan(0);
    expect(sections).toContain('language');
    expect(sections).toEqual([...sections].sort());
  });

  it('无题库的分类返回空数组', () => {
    expect(listSections('grok')).toEqual([]);
  });
});

describe('questions / loadQuestionsFlat', () => {
  it('扁平化后总数等于各 itemId 分组之和', async () => {
    const grouped = await loadQuestionsByCat('cpp');
    const groupedTotal = Object.values(grouped).reduce(
      (sum, list) => sum + list.length,
      0
    );
    const flat = await loadQuestionsFlat('cpp');
    expect(flat.length).toBe(groupedTotal);
    expect(flat.length).toBeGreaterThan(0);
    for (const q of flat) assertQuestionShape(q);
  });

  it('无题库的分类扁平化为空数组', async () => {
    expect(await loadQuestionsFlat('grok')).toEqual([]);
  });
});

describe('questions / loadQuestions（spec 别名）', () => {
  it('按 (cat, itemId) 返回单条题目列表', async () => {
    const list = await loadQuestions('c', 'syntax');
    expect(list.length).toBeGreaterThan(0);
    for (const q of list) assertQuestionShape(q);
  });

  it('不存在的 itemId 返回空数组', async () => {
    expect(await loadQuestions('c', '__nope__')).toEqual([]);
  });
});

describe('questions / 类型收敛', () => {
  it('TechCategory 与 AVAILABLE_CATEGORIES 的差集只有 grok', () => {
    // 文档化约束：AVAILABLE_CATEGORIES ⊆ TechCategory，且 grok 无题库
    const all: TechCategory[] = ['c', 'cpp', 'ds', 'db', 'py', 'linux', 'vdb', 'grok'];
    expect(all.filter((c) => !(AVAILABLE_CATEGORIES as readonly string[]).includes(c)))
      .toEqual(['grok']);
  });
});
