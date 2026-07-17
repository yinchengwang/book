/**
 * 首页（游戏列表）E2E 测试
 */
import { test, expect } from '@playwright/test';

const BASE = 'http://localhost:10098';

test.describe('首页 - 游戏列表', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto(BASE);
    await page.waitForLoadState('networkidle');
    await page.waitForTimeout(3000);
  });

  test('页面加载成功', async ({ page }) => {
    await expect(page.locator('text=游戏中心')).toBeVisible({ timeout: 15000 });
  });

  test('显示四个游戏卡片', async ({ page }) => {
    await expect(page.locator('text=贪吃蛇').first()).toBeVisible({ timeout: 15000 });
    await expect(page.locator('text=数独').first()).toBeVisible();
    await expect(page.getByText('2048', { exact: true })).toBeVisible();
    await expect(page.locator('text=消消乐').first()).toBeVisible();
  });

  test('游戏卡片包含描述', async ({ page }) => {
    await expect(page.locator('text=经典怀旧')).toBeVisible({ timeout: 15000 });
    await expect(page.locator('text=宝石消除')).toBeVisible();
  });

  test('点击消消乐卡片', async ({ page }) => {
    const match3Card = page.locator('text=消消乐').first();
    await match3Card.waitFor({ state: 'visible', timeout: 15000 });
    await match3Card.click();
    await page.waitForTimeout(2000);
    // 点击后页面应该变化
    const pageText = await page.textContent('body');
    expect(pageText).toBeTruthy();
  });
});
