// src/router.tsx
import { lazy, Suspense, type ReactNode } from 'react';
import { createBrowserRouter, RouterProvider } from 'react-router-dom';
import { Layout } from '@/components/Layout';
import { Home } from '@/pages/Home';
import { NotFound } from './pages/NotFound';

// 次要页全部懒加载（首屏 chunk 根治，见 games-redesign-progress Tech Debt）
const Quiz = lazy(() => import('@/pages/Quiz').then((m) => ({ default: m.Quiz })));
const Learn = lazy(() => import('@/pages/Learn').then((m) => ({ default: m.Learn })));
const Kanban = lazy(() => import('@/pages/Kanban').then((m) => ({ default: m.Kanban })));
const Dashboard = lazy(() =>
  import('@/pages/Dashboard').then((m) => ({ default: m.Dashboard }))
);
const FiveYearPlan = lazy(() =>
  import('@/pages/FiveYearPlan').then((m) => ({ default: m.FiveYearPlan }))
);
const Interview = lazy(() =>
  import('@/pages/Interview').then((m) => ({ default: m.Interview }))
);
const InterviewTracker = lazy(() =>
  import('@/pages/InterviewTracker').then((m) => ({ default: m.InterviewTracker }))
);
const Practice = lazy(() =>
  import('@/pages/Practice').then((m) => ({ default: m.Practice }))
);
const Grok = lazy(() => import('@/pages/Grok').then((m) => ({ default: m.Grok })));
const Excerpt = lazy(() =>
  import('@/pages/Excerpt').then((m) => ({ default: m.Excerpt }))
);

function Page({ children }: { children: ReactNode }) {
  return (
    <Suspense
      fallback={
        <div className="p-8 text-center text-sm text-gray-500 dark:text-gray-400">
          页面加载中…
        </div>
      }
    >
      {children}
    </Suspense>
  );
}

const router = createBrowserRouter([
  {
    path: '/',
    element: <Layout />,
    children: [
      { index: true, element: <Home /> },
      { path: 'quiz', element: <Page><Quiz /></Page> },
      { path: 'quiz/:cat', element: <Page><Quiz /></Page> },
      { path: 'quiz/:cat/:item', element: <Page><Quiz /></Page> },
      { path: 'learn', element: <Page><Learn /></Page> },
      { path: 'learn/:cat', element: <Page><Learn /></Page> },
      { path: 'learn/:cat/:item', element: <Page><Learn /></Page> },
      { path: 'kanban', element: <Page><Kanban /></Page> },
      { path: 'dashboard', element: <Page><Dashboard /></Page> },
      { path: 'five-year-plan', element: <Page><FiveYearPlan /></Page> },
      { path: 'interview', element: <Page><Interview /></Page> },
      { path: 'interview-tracker', element: <Page><InterviewTracker /></Page> },
      { path: 'practice', element: <Page><Practice /></Page> },
      { path: 'grok', element: <Page><Grok /></Page> },
      { path: 'excerpt', element: <Page><Excerpt /></Page> },
      { path: '*', element: <NotFound /> }
    ],
  },
]);

export function Router() {
  return <RouterProvider router={router} />;
}
