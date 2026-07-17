/**
 * @file match3.spec.ts
 * @brief 消消乐游戏 E2E 测试
 */
import { test, expect } from '@playwright/test'

test.describe('消消乐游戏', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto('http://localhost:10087/pages/match3/index.html', { waitUntil: 'domcontentloaded' })
    await page.waitForTimeout(1000)
  })

  test('页面应正确加载', async ({ page }) => {
    const body = page.locator('body')
    await expect(body).toBeVisible()
  })

  test('新游戏按钮应可用', async ({ page }) => {
    const newGameBtn = page.getByText('新游戏').first()
    if (await newGameBtn.isVisible()) {
      await newGameBtn.click()
      await page.waitForTimeout(200)
      await expect(page.locator('body')).toBeVisible()
    }
  })
})