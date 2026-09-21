// src/pages/FiveYearPlan/planData.ts
//
// 五年计划打卡系统的数据模型与纯函数层（自 five-year-plan-legacy.html 移植）。
//
// 状态结构与 legacy 页完全一致：
//   { "2026": { "06": { "08": { checks: { [itemId]: boolean }, note: string,
//                               hasLearning?: boolean } } } }
// 因此 localStorage（key 见 usePlanState）在 legacy 页与 React 页之间互通，
// 用户历史数据零迁移。

/** 单日打卡记录 */
export interface DayData {
  checks: Record<string, boolean>;
  note?: string;
  hasLearning?: boolean;
}

/** 全量状态：年 -> 月("01"-"12") -> 日("01"-"31") -> DayData */
export type PlanState = Record<string, Record<string, Record<string, DayData>>>;

export interface CheckItem {
  id: string;
  label: string;
  desc: string;
}

export interface ProjectGroup {
  id: string;
  label: string;
  items: CheckItem[];
}

/** 五大核心工程（每日打卡项），与 legacy CORE_PROJECTS 一致 */
export const CORE_PROJECTS: ProjectGroup[] = [
  { id: 'health', label: '🏗️ 健康基建', items: [
    { id: 'sleep', label: '睡眠 7h+', desc: '不熬夜，作息规律' },
    { id: 'exercise', label: '运动 30min+', desc: '每周至少 3 次' },
    { id: 'diet', label: '饮食规律', desc: '少油少糖，多蛋白' },
    { id: 'posture', label: '颈椎/视力保护', desc: '不久坐，定时远眺' },
  ]},
  { id: 'skill', label: '💪 能力精进', items: [
    { id: 'deepwork', label: '深度学习 2h+', desc: '不可替代的核心技能' },
    { id: 'output', label: '输出笔记/代码', desc: '作品积累，日拱一卒' },
    { id: 'english', label: '英语/专业阅读', desc: '技术文档或论文' },
  ]},
  { id: 'finance', label: '💰 财务管理', items: [
    { id: 'accounting', label: '记账', desc: '记录每笔收支' },
    { id: 'saving', label: '理性消费/储蓄', desc: '非必要不买' },
  ]},
  { id: 'relation', label: '🤝 关系维护', items: [
    { id: 'family', label: '家人沟通', desc: '电话/视频/陪伴' },
    { id: 'social', label: '有效社交', desc: '减少无效社交' },
    { id: 'gratitude', label: '感恩记录', desc: '写下 1 件感恩的事' },
  ]},
  { id: 'cognition', label: '📚 认知拓展', items: [
    { id: 'reading', label: '阅读 30min+', desc: '非技术书籍或深度文章' },
    { id: 'review', label: '每日复盘', desc: '总结今天，规划明天' },
    { id: 'newthing', label: '新知/见识', desc: '学到一个新东西' },
  ]},
];

/** 年度专项打卡项（五大核心工程之外），与 legacy YEAR_EXTRA_ITEMS 一致 */
export const YEAR_EXTRA_ITEMS: Record<number, CheckItem[]> = {
  2026: [
    { id: 'direction', label: '🎯 深耕方向', desc: '花时间探索/确认长期方向' },
    { id: 'habit', label: '🔄 习惯养成', desc: '坚持早起/冥想/写作等新习惯' },
  ],
  2027: [
    { id: 'portfolio', label: '📁 作品输出', desc: '公开文章/开源项目/演讲' },
    { id: 'emotion', label: '🧘 情绪管理', desc: '不冲动，练习正念' },
  ],
  2028: [
    { id: 'sideproject', label: '💼 副业探索', desc: '研究/启动被动收入渠道' },
    { id: 'network', label: '🌐 拓展人脉', desc: '见新朋友/参加行业活动' },
  ],
  2029: [
    { id: 'insurance', label: '🛡️ 保障检查', desc: '保险/体检/养老规划' },
    { id: 'freedom', label: '🕊️ 自由资本', desc: '积累"说不"的底气' },
  ],
  2030: [
    { id: 'mentor', label: '🌟 影响他人', desc: '指导/分享/回馈' },
    { id: 'next5', label: '🗺️ 下个五年', desc: '思考 2031-2035 的方向' },
  ],
};

export const pad2 = (n: number): string => String(n).padStart(2, '0');

export const dateKey = (y: number, m: number, d: number): string =>
  `${y}-${pad2(m)}-${pad2(d)}`;

export const parseDateKey = (key: string): [number, number, number] => {
  const [y, m, d] = key.split('-').map(Number);
  return [y, m, d];
};

export function getDayData(state: PlanState, y: number, m: number, d: number): DayData {
  return state[String(y)]?.[pad2(m)]?.[pad2(d)] ?? { checks: {}, note: '' };
}

/** 返回新 state（不原地修改，配合 React setState） */
export function setDayData(
  state: PlanState,
  y: number,
  m: number,
  d: number,
  data: DayData
): PlanState {
  const ys = String(y);
  const ym = pad2(m);
  const dd = pad2(d);
  return {
    ...state,
    [ys]: {
      ...state[ys],
      [ym]: {
        ...state[ys]?.[ym],
        [dd]: data,
      },
    },
  };
}

/** 删除某日记录，返回新 state */
export function deleteDayData(state: PlanState, y: number, m: number, d: number): PlanState {
  const ys = String(y);
  const ym = pad2(m);
  const dd = pad2(d);
  if (!state[ys]?.[ym]?.[dd]) return state;
  const month = { ...state[ys][ym] };
  delete month[dd];
  const year = { ...state[ys], [ym]: month };
  return { ...state, [ys]: year };
}

/** 统计单日完成数（核心工程 + 该年专项），与 legacy countDayChecks 一致 */
export function countDayChecks(data: DayData, year: number): { done: number; total: number } {
  let total = 0;
  let done = 0;
  for (const proj of CORE_PROJECTS) {
    for (const item of proj.items) {
      total++;
      if (data.checks[item.id]) done++;
    }
  }
  for (const item of YEAR_EXTRA_ITEMS[year] ?? []) {
    total++;
    if (data.checks[item.id]) done++;
  }
  return { done, total };
}

export interface PlanStats {
  todayPct: number;
  monthPct: number;
  streak: number;
  totalDays: number;
}

/** 汇总统计：今日完成率 / 本月完成率 / 连续打卡 / 累计打卡（移植 legacy renderStats） */
export function computeStats(
  state: PlanState,
  today: { year: number; month: number; day: number }
): PlanStats {
  const td = getDayData(state, today.year, today.month, today.day);
  const tc = countDayChecks(td, today.year);
  const todayPct = tc.total > 0 ? Math.round((tc.done / tc.total) * 100) : 0;

  const monthData = state[String(today.year)]?.[pad2(today.month)] ?? {};
  let monthTotal = 0;
  let monthDone = 0;
  for (const data of Object.values(monthData)) {
    const c = countDayChecks(data, today.year);
    monthTotal += c.total;
    monthDone += c.done;
  }
  const monthPct = monthTotal > 0 ? Math.round((monthDone / monthTotal) * 100) : 0;

  let streak = 0;
  const cursor = new Date(today.year, today.month - 1, today.day);
  for (;;) {
    const data = getDayData(state, cursor.getFullYear(), cursor.getMonth() + 1, cursor.getDate());
    if (countDayChecks(data, cursor.getFullYear()).done > 0) {
      streak++;
      cursor.setDate(cursor.getDate() - 1);
    } else {
      break;
    }
  }

  let totalDays = 0;
  for (const [y, months] of Object.entries(state)) {
    for (const days of Object.values(months)) {
      for (const data of Object.values(days)) {
        if (countDayChecks(data, Number(y)).done > 0) totalDays++;
      }
    }
  }

  return { todayPct, monthPct, streak, totalDays };
}

/** 深合并：override 优先（服务器数据覆盖本地），与 legacy deepMerge 一致 */
export function deepMerge<T>(base: T, override: T): T {
  if (
    override !== null &&
    typeof override === 'object' &&
    !Array.isArray(override) &&
    base !== null &&
    typeof base === 'object' &&
    !Array.isArray(base)
  ) {
    const result: Record<string, unknown> = { ...(base as Record<string, unknown>) };
    for (const [key, value] of Object.entries(override as Record<string, unknown>)) {
      result[key] = deepMerge(result[key], value);
    }
    return result as T;
  }
  return override !== undefined ? override : base;
}

/** 月份网格信息：1 日起始星期（0=周日）与当月天数 */
export function monthGrid(year: number, month: number): { firstWeekday: number; daysInMonth: number } {
  return {
    firstWeekday: new Date(year, month - 1, 1).getDay(),
    daysInMonth: new Date(year, month, 0).getDate(),
  };
}

/** 单日内某项目组完成度：done=全绿 / partial=部分 / miss=无 */
export function groupStatus(data: DayData, proj: ProjectGroup): 'done' | 'partial' | 'miss' {
  const done = proj.items.filter((it) => data.checks[it.id]).length;
  if (done === proj.items.length && proj.items.length > 0) return 'done';
  return done > 0 ? 'partial' : 'miss';
}
