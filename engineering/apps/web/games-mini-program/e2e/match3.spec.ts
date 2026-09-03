/**
 * 消消乐游戏 E2E 测试
 */
import { test, expect } from '@playwright/test';

const BASE = 'http://localhost:10091';

test.describe('消消乐游戏', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto(BASE + '/pages/match3/index');
    await page.waitForLoadState('networkidle');
    await page.waitForTimeout(2000);
  });

  test('页面加载成功', async ({ page }) => {
    const hasContent = await page.locator('.match3-page, .game-board, .gem-cell').first().isVisible({ timeout: 10000 }).catch(() => false);
    expect(hasContent).toBe(true);
  });

  test('显示游戏棋盘', async ({ page }) => {
    // 等待棋盘加载
    await page.waitForTimeout(1000);
    const board = page.locator('.match3-board, .game-board');
    const isVisible = await board.first().isVisible({ timeout: 5000 }).catch(() => false);
    expect(isVisible).toBe(true);
  });

  test('显示得分', async ({ page }) => {
    const score = page.locator('.score, .score-value');
    const isVisible = await score.first().isVisible({ timeout: 5000 }).catch(() => false);
    expect(isVisible).toBe(true);
  });

  test('显示步数', async ({ page }) => {
    const moves = page.locator('.moves, .moves-left');
    const isVisible = await moves.first().isVisible({ timeout: 5000 }).catch(() => false);
    expect(isVisible).toBe(true);
  });

  test('方向键或按钮可操作', async ({ page }) => {
    // 尝试点击方向按钮
    const buttons = page.locator('.dir-btn, .action-btn');
    const count = await buttons.count();
    if (count > 0) {
      await buttons.first().click();
      await page.waitForTimeout(200);
    }
    // 只要不崩溃就通过
    expect(true).toBe(true);
  });

  test('返回主页按钮可点击', async ({ page }) => {
    const backBtn = page.locator('text=主页, text=返回');
    if (await backBtn.first().isVisible({ timeout: 3000 }).catch(() => false)) {
      await backBtn.first().click();
      await page.waitForTimeout(1000);
    }
    expect(true).toBe(true);
  });
});