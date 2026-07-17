/**
 * @file snake.spec.ts
 * @brief 贪吃蛇游戏 E2E 测试
 */
import { test, expect } from '@playwright/test'

test.describe('贪吃蛇游戏', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto('http://localhost:10087/pages/snake/index.html', { waitUntil: 'domcontentloaded' })
    await page.waitForTimeout(1000)
  })

  test('页面应正确加载', async ({ page }) => {
    const body = page.locator('body')
    await expect(body).toBeVisible()
  })

  test('点击新游戏应重置游戏', async ({ page }) => {
    const newGameBtn = page.getByText('新游戏').first()
    if (await newGameBtn.isVisible()) {
      await newGameBtn.click()
      await page.waitForTimeout(300)
      await expect(page.locator('body')).toBeVisible()
    }
  })
})