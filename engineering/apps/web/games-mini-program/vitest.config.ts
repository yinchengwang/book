import { defineConfig } from 'vitest/config'
import path from 'path'

export default defineConfig({
  test: {
    // 测试环境
    environment: 'node',
    // 全局测试文件
    include: ['src/utils/**/*.test.ts'],
    // 报告器
    reporters: ['default', 'verbose'],
    // 覆盖率
    coverage: {
      provider: 'v8',
      reporter: ['text', 'json', 'html'],
      include: ['src/utils/**/*.ts'],
      exclude: ['**/*.d.ts', '**/*.test.ts', '**/index.ts'],
    },
    // 测试超时
    testTimeout: 10000,
  },
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'),
    },
  },
})