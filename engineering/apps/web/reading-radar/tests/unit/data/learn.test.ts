// 数据层单元测试：learn.ts（learn-deep Markdown 加载器）
//
// 跑的是仓库内真实 markdown（`?raw` + import.meta.glob）。文件命名
// 约定：data/learn-deep/<cat>/<quadrant>/<cat>-<itemId>.md，itemId
// 用 kebab-case，而 items-registry 里的 id 是 snake_case —— loadLearnContent
// 负责两种形式的桥接，这里用真实存在的 id 验证。

import { describe, expect, it } from 'vitest';
import { listLearnItems, loadLearnContent } from '@/data/learn';

describe('learn / loadLearnContent', () => {
  it('命中存在的文件，返回非空 markdown 内容', async () => {
    const result = await loadLearnContent('c', 'language', 'syntax');
    expect(result.ok).toBe(true);
    if (result.ok) {
      expect(typeof result.content).toBe('string');
      expect(result.content.length).toBeGreaterThan(0);
    }
  });

  it('snake_case id（registry 形式）桥接到 kebab-case 文件', async () => {
    // registry 中 C 语言条目 id 为 control_flow，磁盘文件为 c-control-flow.md
    const result = await loadLearnContent('c', 'language', 'control_flow');
    expect(result.ok).toBe(true);
  });

  it('kebab-case id 原样命中', async () => {
    const result = await loadLearnContent('c', 'language', 'control-flow');
    expect(result.ok).toBe(true);
  });

  it('snake 与 kebab 命中同一份内容', async () => {
    const snake = await loadLearnContent('py', 'language', 'control_flow');
    const kebab = await loadLearnContent('py', 'language', 'control-flow');
    expect(snake).toEqual(kebab);
    expect(snake.ok).toBe(true);
  });

  it('不存在的 itemId 返回 not-found 而不抛错', async () => {
    const result = await loadLearnContent('c', 'language', '__nonexistent__');
    expect(result).toEqual({ ok: false, reason: 'not-found' });
  });

  it('错误象限组合返回 not-found', async () => {
    const result = await loadLearnContent('c', 'systems', 'syntax');
    expect(result).toEqual({ ok: false, reason: 'not-found' });
  });
});

describe('learn / listLearnItems', () => {
  it('列出 (cat, quadrant) 下有内容的 itemId 且已排序', () => {
    const items = listLearnItems('c', 'language');
    expect(items.length).toBeGreaterThan(0);
    expect(items).toContain('syntax');
    expect(items).toContain('control-flow');
    expect(items).toEqual([...items].sort());
  });

  it('全部条目都是字符串且无 cat- 前缀残留', () => {
    for (const item of listLearnItems('py', 'language')) {
      expect(typeof item).toBe('string');
      // 文件名是 py-<itemId>.md，索引时应已剥掉前缀
      expect(item.startsWith('py-')).toBe(false);
    }
  });
});
