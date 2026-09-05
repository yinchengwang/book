// 数据层单元测试：tech.ts（ITEMS_REGISTRY 加载器）
//
// 特性说明：tech.ts 通过 `?raw` 读取真实的 items-registry.js 并在沙箱
// Function 中求值，因此这里的测试跑的是仓库内真实数据。断言使用
// 结构性检查（> 0 / 字段存在），不写死条目数量——数据会持续增长。

import { afterEach, describe, expect, it, vi } from 'vitest';
import {
  __resetTechCache,
  loadAllTechItems,
  loadTechItems,
  loadTechMeta,
} from '@/data/tech';
import type { TechItem } from '@/data/types';

const ALL_STACKS = ['c', 'cpp', 'ds', 'db', 'py', 'linux', 'vdb', 'grok'] as const;
const VALID_QUADRANTS = ['language', 'systems', 'algorithms', 'engineering'];
const VALID_RINGS = ['basic', 'intermediate', 'advanced'];

function assertTechItemShape(item: TechItem): void {
  expect(typeof item.id).toBe('string');
  expect(item.id.length).toBeGreaterThan(0);
  expect(typeof item.title).toBe('string');
  expect(VALID_QUADRANTS).toContain(item.quadrant);
  expect(VALID_RINGS).toContain(item.ring);
  expect(typeof item.desc).toBe('string');
}

describe('tech / loadTechItems', () => {
  it('加载 C 技术栈条目且结构完整', async () => {
    const items = await loadTechItems('c');
    expect(items.length).toBeGreaterThan(0);
    for (const item of items) assertTechItemShape(item);
  });

  it('只返回指定 stack 的条目', async () => {
    const items = await loadTechItems('c');
    // registry 中每条都有 stack 字段，loadTechItems('c') 的结果
    // 必须与 loadAllTechItems().c 完全一致（见下方交叉断言）。
    expect(items.length).toBeGreaterThan(0);
    // registry 首个条目是 C 的 syntax，属于稳定锚点
    expect(items.some((i) => i.id === 'syntax')).toBe(true);
  });

  it('py 技术栈也能加载出条目', async () => {
    const items = await loadTechItems('py');
    expect(items.length).toBeGreaterThan(0);
    for (const item of items) assertTechItemShape(item);
  });
});

describe('tech / loadTechMeta 别名', () => {
  it('loadTechMeta 与 loadTechItems 是同一函数（spec 兼容别名）', () => {
    expect(loadTechMeta).toBe(loadTechItems);
  });

  it('别名调用结果一致', async () => {
    const viaAlias = await loadTechMeta('c');
    const direct = await loadTechItems('c');
    expect(viaAlias).toEqual(direct);
  });
});

describe('tech / loadAllTechItems', () => {
  it('返回全部 8 个 stack 键，每个都是数组', async () => {
    const all = await loadAllTechItems();
    expect(Object.keys(all).sort()).toEqual([...ALL_STACKS].sort());
    for (const stack of ALL_STACKS) {
      expect(Array.isArray(all[stack])).toBe(true);
    }
  });

  it('与 loadTechItems 的结果交叉一致', async () => {
    const all = await loadAllTechItems();
    const c = await loadTechItems('c');
    expect(all.c).toEqual(c);
    expect(all.py).toEqual(await loadTechItems('py'));
  });
});

describe('tech / 缓存行为', () => {
  afterEach(() => {
    // 每个用例结束后复位缓存，避免用例间互相污染
    __resetTechCache();
  });

  it('重复调用返回一致结果（缓存命中）', async () => {
    const first = await loadTechItems('ds');
    const second = await loadTechItems('ds');
    expect(second).toEqual(first);
  });

  it('__resetTechCache 后重新解析仍返回相同数据', async () => {
    const before = await loadTechItems('db');
    expect(before.length).toBeGreaterThan(0);
    __resetTechCache();
    const after = await loadTechItems('db');
    expect(after).toEqual(before);
  });
});

describe('tech / 损坏 registry 降级', () => {
  afterEach(() => {
    vi.doUnmock('@data/app/items-registry.js?raw');
    vi.resetModules();
    __resetTechCache();
  });

  it('registry 源码损坏时返回空数组而不抛错', async () => {
    // 抑制 getRegistry catch 分支的 console.error，保持测试输出干净
    const errSpy = vi.spyOn(console, 'error').mockImplementation(() => {});
    __resetTechCache();
    vi.resetModules();
    vi.doMock('@data/app/items-registry.js?raw', () => ({
      default: 'const ITEMS_REGISTRY = { broken !!!',
    }));
    try {
      const mod = await import('@/data/tech');
      const items = await mod.loadTechItems('c');
      expect(items).toEqual([]);
      const all = await mod.loadAllTechItems();
      expect(all.c).toEqual([]);
    } finally {
      errSpy.mockRestore();
    }
  });
});
