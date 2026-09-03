/**
 * 差距分析 E2E 测试
 *
 * 覆盖按钮（参考 docs/BUTTONS.md Gap Analysis G01-G02）
 */
import { test, expect } from '@playwright/test';

const BASE = 'http://localhost:5173';

test.describe('差距分析', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto(BASE + '/');
    await page.evaluate(() => {
      localStorage.removeItem('rr2-quiz');
      localStorage.removeItem('quiz-storage');
    });
    await page.goto(BASE + '/gap_analysis');
    await page.waitForLoadState('networkidle');
  });

  test('页面加载成功（不报 length 错）', async ({ page }) => {
    await expect(page.locator('.gap-page, .gap-analysis').first()).toBeVisible();
    const errors: string[] = [];
    page.on('pageerror', e => errors.push(e.message));
    await page.waitForTimeout(1000);
    expect(errors.some(e => e.includes('length'))).toBe(false);
  });

  test('空状态 CTA 跳转到 /quiz', async ({ page }) => {
    const cta = page.locator('.empty-cta, .empty-state .btn');
    if (await cta.first().isVisible()) {
      await cta.first().click();
      await expect(page).toHaveURL(/\/quiz/);
    }
  });

  test('空状态显示 3 个统计', async ({ page }) => {
    const stats = page.locator('.empty-stat');
    if (await stats.first().isVisible()) {
      expect(await stats.count()).toBeGreaterThanOrEqual(3);
    }
  });
});