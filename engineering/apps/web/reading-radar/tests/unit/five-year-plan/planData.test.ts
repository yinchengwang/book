// tests/unit/five-year-plan/planData.test.ts
//
// FiveYearPlan 打卡数据层（planData.ts）纯函数测试。
// 断言以结构与语义为主，覆盖 legacy 移植的关键口径：
// 日期键格式、单日统计（核心工程 + 年度专项）、streak/累计、深合并、不可变更新。

import { describe, expect, it } from 'vitest';
import {
  CORE_PROJECTS,
  YEAR_EXTRA_ITEMS,
  computeStats,
  countDayChecks,
  dateKey,
  deepMerge,
  deleteDayData,
  getDayData,
  groupStatus,
  monthGrid,
  parseDateKey,
  setDayData,
  type PlanState,
} from '@/pages/FiveYearPlan/planData';

const FIRST_ITEM = CORE_PROJECTS[0].items[0].id; // 'sleep'
const TOTAL_2026 =
  CORE_PROJECTS.reduce((n, p) => n + p.items.length, 0) + (YEAR_EXTRA_ITEMS[2026]?.length ?? 0);

describe('planData 常量结构', () => {
  it('五大核心工程与 legacy 一致', () => {
    expect(CORE_PROJECTS.map((p) => p.id)).toEqual([
      'health',
      'skill',
      'finance',
      'relation',
      'cognition',
    ]);
    for (const proj of CORE_PROJECTS) {
      expect(proj.items.length).toBeGreaterThan(0);
      const ids = new Set(proj.items.map((i) => i.id));
      expect(ids.size).toBe(proj.items.length);
    }
  });

  it('2026-2030 每年均有专项项', () => {
    for (const y of [2026, 2027, 2028, 2029, 2030]) {
      expect(YEAR_EXTRA_ITEMS[y].length).toBeGreaterThan(0);
    }
  });
});

describe('日期键与读写', () => {
  it('dateKey/parseDateKey 往返一致', () => {
    expect(dateKey(2026, 6, 8)).toBe('2026-06-08');
    expect(parseDateKey('2026-06-08')).toEqual([2026, 6, 8]);
  });

  it('getDayData 缺省返回空记录', () => {
    expect(getDayData({}, 2026, 6, 8)).toEqual({ checks: {}, note: '' });
  });

  it('setDayData 不可变更新且按 y/mm/dd 归位', () => {
    const s0: PlanState = {};
    const s1 = setDayData(s0, 2026, 6, 8, { checks: { [FIRST_ITEM]: true }, note: 'n' });
    expect(s0).toEqual({}); // 原 state 不被修改
    expect(getDayData(s1, 2026, 6, 8).checks[FIRST_ITEM]).toBe(true);
    expect(getDayData(s1, 2026, 6, 8).note).toBe('n');
  });

  it('deleteDayData 删除目标日且不影响其它日', () => {
    let s: PlanState = {};
    s = setDayData(s, 2026, 6, 8, { checks: { a: true } });
    s = setDayData(s, 2026, 6, 9, { checks: { b: true } });
    s = deleteDayData(s, 2026, 6, 8);
    expect(getDayData(s, 2026, 6, 8)).toEqual({ checks: {}, note: '' });
    expect(getDayData(s, 2026, 6, 9).checks.b).toBe(true);
  });
});

describe('countDayChecks', () => {
  it('空记录 done=0, total=核心+专项项数', () => {
    const { done, total } = countDayChecks({ checks: {} }, 2026);
    expect(done).toBe(0);
    expect(total).toBe(TOTAL_2026);
  });

  it('只统计已知项，忽略未知 id', () => {
    const { done, total } = countDayChecks(
      { checks: { [FIRST_ITEM]: true, 'unknown-id': true } },
      2026
    );
    expect(done).toBe(1);
    expect(total).toBe(TOTAL_2026);
  });
});

describe('computeStats', () => {
  it('空状态下全为 0', () => {
    const s = computeStats({}, { year: 2026, month: 6, day: 8 });
    expect(s).toEqual({ todayPct: 0, monthPct: 0, streak: 0, totalDays: 0 });
  });

  it('今日完成率按当日勾选比例', () => {
    let st: PlanState = {};
    st = setDayData(st, 2026, 6, 8, { checks: { [FIRST_ITEM]: true } });
    const s = computeStats(st, { year: 2026, month: 6, day: 8 });
    expect(s.todayPct).toBe(Math.round((1 / TOTAL_2026) * 100));
    expect(s.streak).toBe(1);
    expect(s.totalDays).toBe(1);
  });

  it('连续打卡中断于无记录日', () => {
    let st: PlanState = {};
    st = setDayData(st, 2026, 6, 8, { checks: { [FIRST_ITEM]: true } });
    st = setDayData(st, 2026, 6, 7, { checks: { [FIRST_ITEM]: true } });
    // 6/6 无记录 → streak 只算 8、7 两天
    st = setDayData(st, 2026, 6, 5, { checks: { [FIRST_ITEM]: true } });
    const s = computeStats(st, { year: 2026, month: 6, day: 8 });
    expect(s.streak).toBe(2);
    expect(s.totalDays).toBe(3);
    expect(s.monthPct).toBeGreaterThan(0);
  });
});

describe('deepMerge', () => {
  it('服务器（override）优先，未涉及键保留本地', () => {
    const local: PlanState = { '2026': { '06': { '08': { checks: { a: true }, note: 'local' } } } };
    const server: PlanState = { '2026': { '06': { '08': { checks: { b: true }, note: 'server' } } } };
    const merged = deepMerge(local, server);
    expect(merged['2026']['06']['08'].note).toBe('server');
    expect(merged['2026']['06']['08'].checks).toEqual({ a: true, b: true });
  });

  it('标量与 undefined 处理', () => {
    expect(deepMerge(1, 2)).toBe(2);
    expect(deepMerge(1, undefined as unknown as number)).toBe(1);
  });
});

describe('monthGrid 与 groupStatus', () => {
  it('monthGrid 返回正确的首星期与天数', () => {
    // 2026-06-01 是周一；2026 年 6 月 30 天
    const g = monthGrid(2026, 6);
    expect(g.firstWeekday).toBe(1);
    expect(g.daysInMonth).toBe(30);
    // 闰年二月
    expect(monthGrid(2028, 2).daysInMonth).toBe(29);
  });

  it('groupStatus 三态', () => {
    const proj = CORE_PROJECTS[0];
    expect(groupStatus({ checks: {} }, proj)).toBe('miss');
    expect(groupStatus({ checks: { [proj.items[0].id]: true } }, proj)).toBe('partial');
    const full: Record<string, boolean> = {};
    for (const it of proj.items) full[it.id] = true;
    expect(groupStatus({ checks: full }, proj)).toBe('done');
  });
});
