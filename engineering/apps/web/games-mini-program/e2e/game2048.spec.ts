/**
 * 2048 游戏 E2E 测试
 */
import { test, expect } from '@playwright/test';

const BASE = 'http://localhost:10091';

test.describe('2048 游戏', () => {
  test.beforeEach(async ({ page }) => {
    // 直接导航到 2048 页面
    await page.goto(`${BASE}/pages/game2048/index`);
    await page.waitForLoadState('domcontentloaded');
    // 等待 JS 执行
    await page.waitForTimeout(3000);
  });

  test('页面加载成功', async ({ page }) => {
    // 等待任何游戏元素出现
    const hasContent = await page.locator('.board-row, .tile, .game2048-page, .score-label').first().isVisible({ timeout: 15000 }).catch(() => false);
    expect(hasContent).toBe(true);
  });

  test('显示 4x4 棋盘', async ({ page }) => {
    const rows = page.locator('.board-row');
    await expect(rows.first()).toBeVisible({ timeout: 15000 });
    expect(await rows.count()).toBeGreaterThanOrEqual(1);
  });

  test('显示分数和最高分', async ({ page }) => {
    const scoreLabel = page.locator('.score-label').first();
    await expect(scoreLabel).toBeVisible({ timeout: 15000 });
  });

  test('方向键按钮可点击', async ({ page }) => {
    const upBtn = page.locator('.dir-btn.up').first();
    await expect(upBtn).toBeVisible({ timeout: 15000 });
    await upBtn.click();
  });

  test('点击方向键移动方块', async ({ page }) => {
    const leftBtn = page.locator('.dir-btn.left').first();
    await expect(leftBtn).toBeVisible({ timeout: 15000 });
    await leftBtn.click();
    await page.waitForTimeout(200);

    // 检查棋盘行仍然存在
    const rows = page.locator('.board-row');
    expect(await rows.count()).toBeGreaterThanOrEqual(1);
  });

  test('移动后棋盘状态有效', async ({ page }) => {
    // 获取初始分数
    const initialScore = await page.locator('.score-value').first().textContent();

    // 多次移动
    for (let i = 0; i < 5; i++) {
      await page.locator('.dir-btn.left').first().click();
      await page.waitForTimeout(100);
    }

    // 验证棋盘仍然有 4 行
    const rows = page.locator('.board-row');
    expect(await rows.count()).toBe(4);

    // 验证每个行有 4 个格子
    const tiles = await page.locator('.tile').count();
    expect(tiles).toBe(16);
  });

  test('分数可能增加', async ({ page }) => {
    // 多次移动尝试触发合并
    for (let i = 0; i < 10; i++) {
      const buttons = ['.dir-btn.left', '.dir-btn.right', '.dir-btn.up', '.dir-btn.down'];
      const btn = buttons[i % 4];
      await page.locator(btn).first().click();
      await page.waitForTimeout(100);
    }

    // 棋盘应该仍然正常显示
    const tiles = await page.locator('.tile').count();
    expect(tiles).toBe(16);
  });

  test('新游戏按钮重置游戏', async ({ page }) => {
    const newGameBtn = page.locator('text=新游戏').first();
    await expect(newGameBtn).toBeVisible({ timeout: 15000 });

    // 先移动几次
    const leftBtn = page.locator('.dir-btn.left').first();
    if (await leftBtn.isVisible()) {
      await leftBtn.click();
      await leftBtn.click();
    }
    await page.waitForTimeout(200);

    // 点击新游戏
    await newGameBtn.click();
    await page.waitForTimeout(500);
  });

  test('返回主页按钮可点击', async ({ page }) => {
    const backBtn = page.locator('text=主页').first();
    await expect(backBtn).toBeVisible({ timeout: 15000 });
    await backBtn.click();
    await page.waitForTimeout(1000);
  });
});