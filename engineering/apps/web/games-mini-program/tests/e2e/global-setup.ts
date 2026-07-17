/**
 * @file global-setup.ts
 * @brief Playwright 全局设置 - 启动开发服务器
 */
import { chromium, FullConfig } from '@playwright/test'

export default async function globalSetup(config: FullConfig) {
  // 确保 Playwright 浏览器已安装
  // 这会在第一次运行时自动安装
  const browser = await chromium.launch()
  await browser.close()

  console.log('Playwright 全局设置完成')
}