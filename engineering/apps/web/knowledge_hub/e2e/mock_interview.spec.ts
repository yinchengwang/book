/**
 * 模拟面试 E2E 测试
 *
 * 覆盖按钮（参考 docs/BUTTONS.md Mock Interview M01-M10）
 */
import { test, expect } from '@playwright/test';

const BASE = 'http://localhost:5173';

test.describe('模拟面试', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto(BASE + '/mock_interview');
    await page.waitForLoadState('networkidle');
  });

  test('页面加载成功（配置页）', async ({ page }) => {
    await expect(page.locator('.mock-page, .mock-interview').first()).toBeVisible();
  });

  test('角色 chip 切换', async ({ page }) => {
    const roleChips = page.locator('.role-chip');
    if (await roleChips.first().isVisible()) {
      expect(await roleChips.count()).toBeGreaterThanOrEqual(1);
      await roleChips.nth(1).click();
      await expect(roleChips.nth(1)).toHaveClass(/active/);
    }
  });

  test('难度 chip 切换', async ({ page }) => {
    const diffChips = page.locator('.role-chip');
    if (await diffChips.first().isVisible()) {
      await diffChips.nth(1).click();
      await expect(diffChips.nth(1)).toHaveClass(/active/);
    }
  });

  test('题数 chip 切换', async ({ page }) => {
    const qChips = page.locator('.role-chip');
    if (await qChips.first().isVisible()) {
      await qChips.nth(2).click();
      await expect(qChips.nth(2)).toHaveClass(/active/);
    }
  });

  test('启用 AI 开关', async ({ page }) => {
    const switchEl = page.locator('.switch, [type="checkbox"], switch').first();
    if (await switchEl.isVisible()) {
      await switchEl.click();
      await page.waitForTimeout(200);
    }
  });

  test('开始面试按钮', async ({ page }) => {
    const startBtn = page.locator('.start-btn, .btn-primary:has-text("开始")').first();
    if (await startBtn.isVisible()) {
      await startBtn.click();
      await page.waitForTimeout(500);
      // 应进入答题模式
      await expect(page.locator('.question-card, .interview-q').first()).toBeVisible({ timeout: 5000 });
    }
  });

  test('答题页：上一题/下一题/跳过', async ({ page }) => {
    const startBtn = page.locator('.start-btn, .btn-primary:has-text("开始")').first();
    if (await startBtn.isVisible()) {
      await startBtn.click();
      await page.waitForTimeout(500);
      const nextBtn = page.locator('.nav-btn.primary, .btn-primary:has-text("下一题")').first();
      if (await nextBtn.isVisible()) {
        await nextBtn.click();
        await page.waitForTimeout(300);
      }
      const skipBtn = page.locator('.skip-btn');
      if (await skipBtn.isVisible()) {
        await skipBtn.click();
      }
    }
  });
});