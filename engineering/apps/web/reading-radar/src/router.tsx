// src/router.tsx
import { createBrowserRouter, RouterProvider } from 'react-router-dom';
import { Layout } from '@/components/Layout';
import { Home } from '@/pages/Home';
import { Quiz } from '@/pages/Quiz';
import { Learn } from '@/pages/Learn';
import { Kanban } from '@/pages/Kanban';
import { Dashboard } from '@/pages/Dashboard';
import { FiveYearPlan } from '@/pages/FiveYearPlan';
import { Interview } from '@/pages/Interview';
import { InterviewTracker } from '@/pages/InterviewTracker';
import { Practice } from '@/pages/Practice';
import { Grok } from '@/pages/Grok';
import { Excerpt } from '@/pages/Excerpt';
import { NotFound } from './pages/NotFound';

const router = createBrowserRouter([
  {
    path: '/',
    element: <Layout />,
    children: [
      { index: true, element: <Home /> },
      { path: 'quiz', element: <Quiz /> },
      { path: 'quiz/:cat/:item', element: <Quiz /> },
      { path: 'learn', element: <Learn /> },
      { path: 'learn/:cat', element: <Learn /> },
      { path: 'learn/:cat/:item', element: <Learn /> },
      { path: 'kanban', element: <Kanban /> },
      { path: 'dashboard', element: <Dashboard /> },
      { path: 'five-year-plan', element: <FiveYearPlan /> },
      { path: 'interview', element: <Interview /> },
      { path: 'interview-tracker', element: <InterviewTracker /> },
      { path: 'practice', element: <Practice /> },
      { path: 'grok', element: <Grok /> },
      { path: 'excerpt', element: <Excerpt /> },
      { path: '*', element: <NotFound /> }
    ],
  },
]);

export function Router() {
  return <RouterProvider router={router} />;
}
