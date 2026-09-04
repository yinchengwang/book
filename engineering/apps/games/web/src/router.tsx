// src/router.tsx
import { createBrowserRouter, RouterProvider } from 'react-router-dom';
import { Home } from '@/pages/Home';
import { Game2048 } from '@/pages/Game2048';
import { Snake } from '@/pages/Snake';
import { Sudoku } from '@/pages/Sudoku';

const router = createBrowserRouter([
  { path: '/', element: <Home /> },
  { path: '/2048', element: <Game2048 /> },
  { path: '/snake', element: <Snake /> },
  { path: '/sudoku', element: <Sudoku /> },
]);

export function Router() {
  return <RouterProvider router={router} />;
}