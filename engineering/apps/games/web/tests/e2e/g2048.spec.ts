// tests/e2e/g2048.spec.ts
import { test, expect } from '@playwright/test';

test('2048: 能玩并计分', async ({ page }) => {
  await page.goto('/2048');
  await expect(page.locator('canvas')).toBeVisible();
  // 玩 30 步，每步间隔 60ms
  for (let i = 0; i < 30; i++) {
    await page.keyboard.press(['ArrowUp', 'ArrowRight', 'ArrowDown', 'ArrowLeft'][i % 4]!);
    await page.waitForTimeout(60);
  }
  // 验证分数区域仍可见（说明没崩溃）
  await expect(page.getByText(/分数/)).toBeVisible();
});

test('2048: 首页加载游戏中心', async ({ page }) => {
  await page.goto('/');
  await expect(page.getByText('贪吃蛇')).toBeVisible();
  await expect(page.getByText('数独')).toBeVisible();
});

test('2048: 返回首页', async ({ page }) => {
  await page.goto('/2048');
  await page.getByText(/返回首页/).click();
  await expect(page).toHaveURL(/\/$/);
});
