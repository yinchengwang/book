// src/pages/InterviewTracker/index.tsx
//
// MVP-6.1 Interview Tracker — read-only company pipeline view.
//
// Data: `data/interview-tracker/_index.json` (and per-company `_meta.md`
// for details). For MVP we only read the index.
//
// The legacy page is full CRUD; here we just render the snapshot so the
// route is reachable and the data is discoverable.

import { useMemo } from 'react';
import { Card } from '@shared/ui/Card';
import indexSource from '@data/interview-tracker/_index.json?raw';

interface CompanyEntry {
  companyId: string;
  company: string;
  position: string;
  city?: string;
  status: string;
  statusDate?: string;
  createdAt?: string;
  updatedAt?: string;
  salaryBase?: number;
  salaryMonths?: number;
  stack?: string;
  direction?: string;
}

interface TrackerIndex {
  version?: number;
  lastModified?: string;
  companies: CompanyEntry[];
}

const STATUS_LABEL: Record<string, { label: string; cls: string }> = {
  intention: {
    label: '意向沟通',
    cls: 'bg-gray-100 text-gray-700 dark:bg-gray-800 dark:text-gray-300',
  },
  communicating: {
    label: '初步沟通',
    cls: 'bg-blue-100 text-blue-700 dark:bg-blue-900/40 dark:text-blue-300',
  },
  applied: {
    label: '已投递',
    cls: 'bg-indigo-100 text-indigo-700 dark:bg-indigo-900/40 dark:text-indigo-300',
  },
  resume_screen: {
    label: '简历筛选',
    cls: 'bg-violet-100 text-violet-700 dark:bg-violet-900/40 dark:text-violet-300',
  },
  first_interview: {
    label: '一面',
    cls: 'bg-amber-100 text-amber-700 dark:bg-amber-900/40 dark:text-amber-300',
  },
  second_interview: {
    label: '二面',
    cls: 'bg-orange-100 text-orange-700 dark:bg-orange-900/40 dark:text-orange-300',
  },
  third_interview: {
    label: '三面',
    cls: 'bg-pink-100 text-pink-700 dark:bg-pink-900/40 dark:text-pink-300',
  },
  offer: {
    label: 'Offer',
    cls: 'bg-emerald-100 text-emerald-700 dark:bg-emerald-900/40 dark:text-emerald-300',
  },
  rejected: {
    label: '已拒',
    cls: 'bg-rose-100 text-rose-700 dark:bg-rose-900/40 dark:text-rose-300',
  },
};

function parseIndex(raw: string): TrackerIndex {
  try {
    return JSON.parse(raw) as TrackerIndex;
  } catch {
    return { companies: [] };
  }
}

const indexData: TrackerIndex = parseIndex(indexSource);

export function InterviewTracker() {
  const companies = useMemo(() => indexData.companies ?? [], []);

  return (
    <div className="max-w-5xl mx-auto space-y-4">
      <div>
        <h1 className="text-2xl font-bold mb-1">🏢 面试追踪</h1>
        <p className="text-sm text-gray-500 dark:text-gray-400">
          {companies.length} 家公司 · 只读视图
        </p>
      </div>

      {companies.length === 0 ? (
        <Card className="p-6 text-center text-gray-500 dark:text-gray-400">
          暂无面试记录
        </Card>
      ) : (
        <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
          {companies.map((c) => {
            const status = STATUS_LABEL[c.status] ?? {
              label: c.status,
              cls: 'bg-gray-100 text-gray-600',
            };
            const salary =
              c.salaryBase && c.salaryMonths
                ? `${c.salaryBase}K × ${c.salaryMonths}`
                : null;
            return (
              <Card key={c.companyId} className="p-4">
                <div className="flex items-start justify-between gap-3 mb-2">
                  <div>
                    <h3 className="font-semibold text-lg text-gray-900 dark:text-gray-100">
                      {c.company}
                    </h3>
                    <p className="text-sm text-gray-500 dark:text-gray-400 mt-0.5">
                      {c.position}
                      {c.city ? ` · ${c.city}` : ''}
                    </p>
                  </div>
                  <span
                    className={`shrink-0 px-2 py-0.5 rounded text-xs font-medium ${status.cls}`}
                  >
                    {status.label}
                  </span>
                </div>
                <div className="flex flex-wrap gap-2 text-xs text-gray-500 dark:text-gray-400">
                  {c.stack && (
                    <span className="px-2 py-0.5 rounded bg-gray-100 dark:bg-gray-800">
                      {c.stack}
                    </span>
                  )}
                  {c.direction && <span>方向: {c.direction}</span>}
                  {salary && <span>薪资: {salary}</span>}
                </div>
                {c.statusDate && (
                  <p className="text-xs text-gray-400 dark:text-gray-500 mt-2">
                    最近更新: {c.statusDate}
                  </p>
                )}
              </Card>
            );
          })}
        </div>
      )}
    </div>
  );
}
