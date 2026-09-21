// src/pages/FiveYearPlan/DayEditor.tsx
//
// 单日打卡编辑弹窗：五大核心工程 + 年度专项勾选、笔记、保存/清除/取消
// （移植 legacy openModal/saveModal/deleteDayData；学习联动区属另一子系统，不移植）。
import { useEffect, useState } from 'react';
import { Button } from '@shared/ui/Button';
import {
  CORE_PROJECTS,
  YEAR_EXTRA_ITEMS,
  parseDateKey,
  type DayData,
} from './planData';

interface DayEditorProps {
  dateKeyStr: string; // "YYYY-MM-DD"
  initial: DayData;
  onSave: (y: number, m: number, d: number, data: DayData) => void;
  onClear: (y: number, m: number, d: number) => void;
  onClose: () => void;
}

export function DayEditor({ dateKeyStr, initial, onSave, onClear, onClose }: DayEditorProps) {
  const [y, m, d] = parseDateKey(dateKeyStr);
  const [checks, setChecks] = useState<Record<string, boolean>>(initial.checks);
  const [note, setNote] = useState(initial.note ?? '');

  // Esc 关闭
  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      if (e.key === 'Escape') onClose();
    };
    document.addEventListener('keydown', handler);
    return () => document.removeEventListener('keydown', handler);
  }, [onClose]);

  const toggle = (id: string) => setChecks((prev) => ({ ...prev, [id]: !prev[id] }));
  const extras = YEAR_EXTRA_ITEMS[y] ?? [];

  const handleSave = () => {
    onSave(y, m, d, { checks, note, hasLearning: initial.hasLearning });
    onClose();
  };

  const handleClear = () => {
    if (window.confirm('确定要清除这一天的所有打卡记录吗？')) {
      onClear(y, m, d);
      onClose();
    }
  };

  const renderGroup = (title: string, items: { id: string; label: string; desc: string }[]) => (
    <div key={title}>
      <h4 className="text-sm font-semibold text-gray-900 dark:text-gray-100 mb-1.5">{title}</h4>
      <div className="space-y-1">
        {items.map((item) => (
          <label
            key={item.id}
            className="flex items-baseline gap-2 cursor-pointer p-1 rounded hover:bg-gray-50 dark:hover:bg-gray-800/60"
          >
            <input
              type="checkbox"
              checked={!!checks[item.id]}
              onChange={() => toggle(item.id)}
              className="w-4 h-4 accent-emerald-500 shrink-0 translate-y-0.5"
            />
            <span className="text-sm text-gray-700 dark:text-gray-200">{item.label}</span>
            <span className="text-xs text-gray-400">{item.desc}</span>
          </label>
        ))}
      </div>
    </div>
  );

  return (
    <div
      className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 p-4"
      onClick={onClose}
    >
      <div
        role="dialog"
        aria-label={`${y}年${m}月${d}日 打卡记录`}
        className="w-full max-w-lg max-h-[85vh] overflow-y-auto rounded-lg bg-white dark:bg-gray-800 p-5 shadow-xl space-y-4"
        onClick={(e) => e.stopPropagation()}
      >
        <h3 className="text-lg font-bold text-gray-900 dark:text-gray-100">
          📋 {y}年{m}月{d}日 打卡记录
        </h3>

        {CORE_PROJECTS.map((proj) => renderGroup(proj.label, proj.items))}
        {extras.length > 0 && renderGroup(`🎯 ${y}年度专项`, extras)}

        <textarea
          value={note}
          onChange={(e) => setNote(e.target.value)}
          placeholder="✍️ 今日笔记/复盘..."
          rows={3}
          className="w-full rounded-md border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-900 p-2 text-sm resize-y focus:outline-none focus:ring-2 focus:ring-primary-500"
        />

        <div className="flex gap-2 justify-end">
          <Button variant="ghost" size="sm" onClick={handleClear}>
            🗑️ 清除
          </Button>
          <Button variant="ghost" size="sm" onClick={onClose}>
            取消
          </Button>
          <Button variant="primary" size="sm" onClick={handleSave}>
            💾 保存
          </Button>
        </div>
      </div>
    </div>
  );
}
