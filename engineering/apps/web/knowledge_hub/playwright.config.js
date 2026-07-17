/**
 * Playwright E2E 测试配置（Reading Radar 2.0 Vite 端）
 *
 * - baseURL 指向 Vite dev server (5173)
 * - 浏览器复用本地 ms-playwright 缓存
 * - Windows 平台走 chrome.exe（完整 Chromium）而非 headless_shell，
 *   避免 --remote-debugging-pipe 模式卡住
 * - 不自动启动 webServer（开发时 Vite 单独跑）
 */
const { defineConfig, devices } = require('@playwright/test');

// 优先用完整 Chromium，headless_shell 在 Windows 上 pipe 通信易卡
const CHROME_FULL = 'C:\\Users\\yinch\\AppData\\Local\\ms-playwright\\chromium-1223\\chrome-win64\\chrome.exe';
const CHROME_HEADLESS = 'C:\\Users\\yinch\\AppData\\Local\\ms-playwright\\chromium_headless_shell-1223\\chrome-headless-shell-win64\\chrome-headless-shell.exe';

// 探测：用完整 Chromium
const fs = require('fs');
const CHROME = fs.existsSync(CHROME_FULL) ? CHROME_FULL : CHROME_HEADLESS;

module.exports = defineConfig({
  testDir: './e2e',
  fullyParallel: false,        // 串行避免 localStorage 互相干扰
  forbidOnly: !!process.env.CI,
  retries: 0,                  // 失败立即暴露，不重试
  workers: 1,                  // 单线程
  reporter: 'line',
  timeout: 60000,              // 单测超时 60s（Vite 首屏冷启动较慢）

  use: {
    baseURL: 'http://localhost:5173',
    trace: 'retain-on-failure',
    screenshot: 'only-on-failure',
    headless: true,
    actionTimeout: 10000,
    navigationTimeout: 30000,
    launchOptions: {
      executablePath: CHROME,
      args: [
        '--no-sandbox',
        '--disable-dev-shm-usage',
        '--disable-gpu',
        '--disable-software-rasterizer',
        // 关键：禁用 pipe 模式，走 TCP 端口通信（Windows 兼容）
        '--remote-debugging-port=0'
      ]
    }
  },

  projects: [
    {
      name: 'chromium',
      use: { ...devices['Desktop Chrome'] },
    }
  ]
});

