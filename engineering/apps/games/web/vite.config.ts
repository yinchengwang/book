import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import path from 'path';

export default defineConfig({
  appType: 'spa',
  plugins: [react()],
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'),
      '@shared': path.resolve(__dirname, './shared/web/src'), // cross-project shared design system (active since MVP-4)
    },
  },
  server: {
    port: 5173,
  },
  test: {
    include: ['tests/unit/**/*.test.ts'],
  },
});
