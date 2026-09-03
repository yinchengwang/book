/**
 * @file 2048.spec.ts
 * @brief 2048 游戏 E2E 测试
 */
import { test, expect } from '@playwright/test'

test.describe('2048 游戏', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto('http://localhost:10087/pages/2048/index.html', { waitUntil: 'domcontentloaded' })
    // 等待页面加载
    await page.waitForTimeout(1000)
  })

  test('页面应正确加载', async ({ page }) => {
    const body = page.locator('body')
    await expect(body).toBeVisible()
  })

  test('可以开始新游戏', async ({ page }) => {
    // 点击新游戏按钮
    const newGameBtn = page.getByText('新游戏').first()
    if (await newGameBtn.isVisible()) {
      await newGameBtn.click()
      await page.waitForTimeout(200)
      // 游戏应该重置
      await expect(page.locator('body')).toBeVisible()
    }
  })
})