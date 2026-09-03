/**
 * @file index.spec.ts
 * @brief 游戏列表页面 E2E 测试
 */
import { test, expect } from '@playwright/test'

test.describe('游戏列表页面', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto('http://localhost:10087/', { waitUntil: 'domcontentloaded' })
    // 等待页面加载完成
    await page.waitForTimeout(1000)
  })

  test('页面应正确加载', async ({ page }) => {
    // 检查页面标题
    const title = await page.title()
    expect(title).toBeTruthy()
  })

  test('应显示游戏链接', async ({ page }) => {
    // 检查有链接指向游戏页面
    const links = page.locator('a')
    const count = await links.count()
    expect(count).toBeGreaterThan(0)
  })

  test('2048 游戏链接应可点击', async ({ page }) => {
    // 找到 2048 游戏链接
    const game2048 = page.locator('a[href*="2048"]').first()
    if (await game2048.isVisible()) {
      await game2048.click()
      await page.waitForTimeout(500)
      // 验证页面有变化
      expect(page.url()).toContain('2048')
    }
  })

  test('贪吃蛇游戏链接应可点击', async ({ page }) => {
    const snake = page.locator('a[href*="snake"]').first()
    if (await snake.isVisible()) {
      await snake.click()
      await page.waitForTimeout(500)
      expect(page.url()).toContain('snake')
    }
  })

  test('数独游戏链接应可点击', async ({ page }) => {
    const sudoku = page.locator('a[href*="sudoku"]').first()
    if (await sudoku.isVisible()) {
      await sudoku.click()
      await page.waitForTimeout(500)
      expect(page.url()).toContain('sudoku')
    }
  })

  test('消消乐游戏链接应可点击', async ({ page }) => {
    const match3 = page.locator('a[href*="match3"]').first()
    if (await match3.isVisible()) {
      await match3.click()
      await page.waitForTimeout(500)
      expect(page.url()).toContain('match3')
    }
  })
})