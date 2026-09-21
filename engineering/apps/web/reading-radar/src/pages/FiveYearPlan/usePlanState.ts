// src/pages/FiveYearPlan/usePlanState.ts
//
// 打卡状态 hook：localStorage 为主，Express state API 可选同步，捆绑快照兜底播种。
//
// - localStorage key 与 legacy 页相同（five_year_plan_state），两页数据互通
// - 服务器 key 用 five-year-plan-state（与 user-data/state/ 实际文件名一致；
//   legacy 页的 five-year-plan key 与磁盘文件对不上，属既有偏差，此处纠正）
// - 服务器不可达时全部静默降级为纯本地（与 legacy 行为一致）

import { useCallback, useEffect, useRef, useState } from 'react';
import {
  deepMerge,
  deleteDayData,
  getDayData,
  setDayData,
  type DayData,
  type PlanState,
} from './planData';
import { safeGet, safeSet } from '@shared/storage/safeStorage';
import snapshotRaw from '../../../user-data/state/five-year-plan-state.json?raw';

const STORAGE_KEY = 'five_year_plan_state';
const API_KEY = 'five-year-plan-state';
const API_BASE =
  typeof window !== 'undefined' &&
  (window.location.protocol === 'http:' || window.location.protocol === 'https:')
    ? `${window.location.origin}/api`
    : null;

function seedState(): PlanState {
  const local = safeGet<PlanState | null>(STORAGE_KEY, null);
  if (local && Object.keys(local).length > 0) return local;
  try {
    const bundled = JSON.parse(snapshotRaw) as PlanState;
    return bundled && typeof bundled === 'object' ? bundled : {};
  } catch {
    return {};
  }
}

export function usePlanState() {
  const [state, setState] = useState<PlanState>(seedState);
  const saveTimer = useRef<ReturnType<typeof setTimeout> | null>(null);

  // 挂载时从服务器同步（服务器数据优先，多设备合并）
  useEffect(() => {
    if (!API_BASE) return;
    let cancelled = false;
    (async () => {
      try {
        const res = await fetch(`${API_BASE}/state/${API_KEY}`);
        if (!res.ok) return;
        const serverData = (await res.json()) as PlanState;
        if (!serverData || Object.keys(serverData).length === 0) return;
        if (cancelled) return;
        setState((prev) => {
          const merged = deepMerge(prev, serverData);
          safeSet(STORAGE_KEY, merged);
          return merged;
        });
      } catch {
        /* 静默：离线或服务器未启动 */
      }
    })();
    return () => {
      cancelled = true;
    };
  }, []);

  // 持久化：localStorage 立即写，服务器防抖 500ms（移植 legacy _apiSave）
  const persist = useCallback((next: PlanState) => {
    safeSet(STORAGE_KEY, next);
    if (!API_BASE) return;
    if (saveTimer.current) clearTimeout(saveTimer.current);
    saveTimer.current = setTimeout(() => {
      fetch(`${API_BASE}/state/${API_KEY}`, {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(next),
      }).catch(() => {
        /* 静默失败 */
      });
    }, 500);
  }, []);

  const updateDay = useCallback(
    (y: number, m: number, d: number, data: DayData) => {
      setState((prev) => {
        const next = setDayData(prev, y, m, d, data);
        persist(next);
        return next;
      });
    },
    [persist]
  );

  const clearDay = useCallback(
    (y: number, m: number, d: number) => {
      setState((prev) => {
        const next = deleteDayData(prev, y, m, d);
        persist(next);
        return next;
      });
    },
    [persist]
  );

  const toggleCheck = useCallback(
    (y: number, m: number, d: number, itemId: string) => {
      setState((prev) => {
        const current = getDayData(prev, y, m, d);
        const next = setDayData(prev, y, m, d, {
          ...current,
          checks: { ...current.checks, [itemId]: !current.checks[itemId] },
        });
        persist(next);
        return next;
      });
    },
    [persist]
  );

  return { state, updateDay, clearDay, toggleCheck };
}
