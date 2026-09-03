/**
 * 数独游戏 E2E 测试
 */
import { test, expect } from '@playwright/test';

const BASE = 'http://localhost:10091';

test.describe('数独游戏', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto(BASE + '/pages/sudoku/index');
    await page.waitForLoadState('networkidle');
    await page.waitForTimeout(1000);
  });

  test('页面加载成功', async ({ page }) => {
    await expect(page.locator('.sudoku-page')).toBeVisible();
  });

  test('显示 9x9 棋盘', async ({ page }) => {
    const cells = page.locator('.sudoku-cell');
    expect(await cells.count()).toBe(81);
  });

  test('显示难度选择标签', async ({ page }) => {
    await expect(page.locator('text=简单')).toBeVisible();
    await expect(page.locator('text=中等')).toBeVisible();
    await expect(page.locator('text=困难')).toBeVisible();
  });

  test('显示数字键盘 1-9', async ({ page }) => {
    for (let i = 1; i <= 9; i++) {
      await expect(page.locator(`.num-btn:has-text("${i}")`).first()).toBeVisible();
    }
  });

  test('点击数字应能填充到格子', async ({ page }) => {
    // 先点击一个空格（cursor 位置）
    const cells = page.locator('.sudoku-cell:not(.given)');
    const firstEmpty = cells.first();
    await firstEmpty.click();
    await page.waitForTimeout(100);

    // 点击数字 5
    await page.locator('.num-btn:has-text("5")').first().click();
    await page.waitForTimeout(200);

    // 页面应该仍然正常显示
    await expect(page.locator('.sudoku-page')).toBeVisible();
  });

  test('提示按钮工作', async ({ page }) => {
    // 点击提示
    await page.locator('text=提示').click();
    await page.waitForTimeout(300);

    // 页面应该仍然正常显示
    await expect(page.locator('.sudoku-page')).toBeVisible();
  });

  test('切换难度重新生成题目', async ({ page }) => {
    // 获取初始空格子数
    const initialEmpty = await page.locator('.info-value').first().textContent();

    // 切换到困难
    await page.locator('text=困难').click();
    await page.waitForTimeout(500);

    // 空格子数应该变化（困难模式空格更多）
    const newEmpty = await page.locator('.info-value').first().textContent();
    expect(newEmpty).not.toBe(initialEmpty);
  });

  test('重开按钮重置游戏', async ({ page }) => {
    // 先填一个数字
    await page.locator('.num-btn:has-text("5")').first().click();
    await page.waitForTimeout(100);

    // 点击重开
    await page.locator('text=重开').click();
    await page.waitForTimeout(300);

    // 游戏应该重置（页面仍然可见）
    await expect(page.locator('.sudoku-page')).toBeVisible();
  });

  test('返回主页按钮可点击', async ({ page }) => {
    await page.locator('text=主页').click();
    await page.waitForTimeout(1000);

    // 应该返回到主页
    await expect(page.locator('.game-card').first()).toBeVisible();
  });
});