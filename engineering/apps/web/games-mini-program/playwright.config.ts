/**
 * @file playwright.config.ts
 * @brief Playwright E2E 测试配置
 */
import { defineConfig, devices } from '@playwright/test'

export default defineConfig({
  // 测试目录
  testDir: './tests/e2e',
  // 严格模式
  fullyParallel: true,
  forbidOnly: !!process.env.CI,
  retries: process.env.CI ? 2 : 0,
  workers: process.env.CI ? 1 : undefined,
  reporter: 'html',

  // 全局超时
  timeout: 30000,
  expect: {
    timeout: 5000
  },

  // 基础 URL
  baseURL: 'http://localhost:10087',

  // 全局预处理
  globalSetup: require.resolve('./tests/e2e/global-setup.ts'),

  // 使用的项目
  projects: [
    // Mobile Safari
    {
      name: 'Mobile Chrome',
      use: {
        ...devices['Pixel 5'],
      },
    },
    // Desktop Chrome
    {
      name: 'Desktop Chrome',
      use: {
        ...devices['Desktop Chrome'],
        // 截图目录
        screenshot: 'only-on-failure',
      },
    },
  ],

  // Web 服务器配置 - 注释掉，使用 CI 环境变量控制
  // webServer: {
  //   command: 'npm run dev:h5',
  //   url: 'http://localhost:10087',
  //   reuseExistingServer: !process.env.CI,
  //   timeout: 120000,
  // },

  // 输出目录
  outputDir: 'tests/e2e/test-results',
})