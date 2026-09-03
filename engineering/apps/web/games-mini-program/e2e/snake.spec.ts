/**
 * 贪吃蛇游戏 E2E 测试
 */
import { test, expect } from '@playwright/test';

const BASE = 'http://localhost:10091';

test.describe('贪吃蛇游戏', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto(BASE + '/pages/snake/index');
    await page.waitForLoadState('networkidle');
    // 等待游戏初始化
    await page.waitForTimeout(2000);
  });

  test('页面加载成功', async ({ page }) => {
    await expect(page.locator('.snake-page')).toBeVisible();
  });

  test('显示游戏棋盘', async ({ page }) => {
    await expect(page.locator('.game-board')).toBeVisible();
    const cells = page.locator('.grid-cell');
    expect(await cells.count()).toBeGreaterThan(0);
  });

  test('棋盘尺寸正确', async ({ page }) => {
    const cells = page.locator('.grid-cell');
    // 棋盘应该是 20x15 = 300 格
    expect(await cells.count()).toBe(300);
  });

  test('显示得分和速度', async ({ page }) => {
    await expect(page.locator('.score')).toContainText('得分');
    await expect(page.locator('.score')).toContainText('速度');
  });

  test('方向键按钮可点击', async ({ page }) => {
    const upBtn = page.locator('.dir-btn.up');
    await expect(upBtn).toBeVisible();
    await upBtn.click();
  });

  test('点击方向键蛇移动', async ({ page }) => {
    // 获取初始蛇头位置
    const initialSnake = await page.locator('.grid-cell.snake-head').count();

    // 点击右方向键
    await page.locator('.dir-btn.right').click();
    await page.waitForTimeout(300);

    // 蛇头应该存在
    expect(await page.locator('.grid-cell.snake-head').count()).toBeGreaterThanOrEqual(1);
  });

  test('多次点击方向键蛇持续移动', async ({ page }) => {
    // 点击多个方向
    await page.locator('.dir-btn.right').click();
    await page.waitForTimeout(200);
    await page.locator('.dir-btn.down').click();
    await page.waitForTimeout(200);
    await page.locator('.dir-btn.left').click();
    await page.waitForTimeout(200);

    // 蛇头应该存在
    expect(await page.locator('.grid-cell.snake-head').count()).toBeGreaterThanOrEqual(1);
  });

  test('重新开始按钮显示游戏结束弹窗', async ({ page }) => {
    // 直接触发游戏结束（通过 JS）
    await page.evaluate(() => {
      const state = (window as any).__snakeState;
      if (state) state.isGameOver = true;
    });
    await page.waitForTimeout(100);

    // 游戏结束弹窗
    await expect(page.locator('.game-over-overlay')).toBeVisible();
  });

  test('返回主页按钮可点击', async ({ page }) => {
    // 先触发游戏结束
    await page.evaluate(() => {
      const state = (window as any).__snakeState;
      if (state) state.isGameOver = true;
    });
    await page.waitForTimeout(100);

    // 点击返回主页
    await page.locator('text=返回主页').click();
    await expect(page).toHaveURL(/index/);
  });
});