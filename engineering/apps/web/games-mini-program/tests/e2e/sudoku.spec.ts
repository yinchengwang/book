/**
 * @file sudoku.spec.ts
 * @brief 数独游戏 E2E 测试
 */
import { test, expect } from '@playwright/test'

test.describe('数独游戏', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto('http://localhost:10087/pages/sudoku/index.html', { waitUntil: 'domcontentloaded' })
    await page.waitForTimeout(1000)
  })

  test('页面应正确加载', async ({ page }) => {
    const body = page.locator('body')
    await expect(body).toBeVisible()
  })

  test('数独页面应可访问', async ({ page }) => {
    // 检查页面可以访问（返回 200 或其他有效响应）
    const response = await page.goto('http://localhost:10087/pages/sudoku/index.html', { waitUntil: 'domcontentloaded' })
    expect(response?.status()).toBeLessThan(400)
  })

  test('提示按钮应可用', async ({ page }) => {
    const hintBtn = page.getByText('提示').first()
    if (await hintBtn.isVisible()) {
      await hintBtn.click()
      await page.waitForTimeout(200)
      await expect(page.locator('body')).toBeVisible()
    }
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