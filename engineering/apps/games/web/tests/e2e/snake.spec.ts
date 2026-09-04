// tests/e2e/snake.spec.ts
import { test, expect } from '@playwright/test';

test('snake: 加载并显示棋盘', async ({ page }) => {
  await page.goto('/snake');
  await expect(page.locator('canvas')).toBeVisible();
  await expect(page.getByText(/贪吃蛇/)).toBeVisible();
});

test('snake: 方向键响应', async ({ page }) => {
  await page.goto('/snake');
  await page.waitForTimeout(300); // 等 RAF 启动
  // 不期望崩溃即可
  await page.keyboard.press('ArrowUp');
  await page.waitForTimeout(200);
  await expect(page.locator('canvas')).toBeVisible();
});
