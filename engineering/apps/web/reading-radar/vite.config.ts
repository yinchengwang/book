/// <reference types="vitest" />
import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import path from 'path';

export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'),
      '@data': path.resolve(__dirname, './data'),
      '@shared': path.resolve(__dirname, '../../games/web/shared/web/src'),
    },
  },
  // vitest 配置：单元测试 + 覆盖率口径锁定在 src/data
  test: {
    include: ['tests/unit/**/*.test.ts'],
    coverage: {
      provider: 'v8',
      reporter: ['text', 'html'],
      // 覆盖率口径：仅数据层（UI 组件不在本任务范围）
      include: ['src/data/**'],
      thresholds: { lines: 90, statements: 90 },
    },
  },
  build: {
    chunkSizeWarningLimit: 1200,
  },
});
