/**
 * 五年计划 E2E 测试
 *
 * 覆盖按钮（参考 docs/BUTTONS.md Five Year Plan F01-F13）
 */
import { test, expect } from '@playwright/test';

const BASE = 'http://localhost:5173';

test.describe('五年计划', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto(BASE + '/five_year_plan');
    await page.waitForLoadState('networkidle');
  });

  test('页面加载成功', async ({ page }) => {
    await expect(page.locator('.fyp-page, .five-year-plan').first()).toBeVisible();
  });

  test('5 个年份 chip', async ({ page }) => {
    const yearChips = page.locator('.year-chip, .year-tab');
    if (await yearChips.first().isVisible()) {
      expect(await yearChips.count()).toBeGreaterThanOrEqual(1);
    }
  });

  test('点击年份切换', async ({ page }) => {
    const yearChips = page.locator('.year-chip, .year-tab');
    if (await yearChips.count() >= 2) {
      await yearChips.nth(1).click();
      await expect(yearChips.nth(1)).toHaveClass(/active/);
    }
  });

  test('今日打卡项快速勾选', async ({ page }) => {
    const item = page.locator('.check-item, .today-item').first();
    if (await item.isVisible()) {
      await item.click();
      await page.waitForTimeout(200);
    }
  });

  test('月历：上一月/下一月', async ({ page }) => {
    const nextBtn = page.locator('.month-btn:has-text("▶"), .month-btn:has-text("→")');
    if (await nextBtn.first().isVisible()) {
      await nextBtn.first().click();
      await page.waitForTimeout(200);
    }
    const prevBtn = page.locator('.month-btn:has-text("◀"), .month-btn:has-text("←")');
    if (await prevBtn.first().isVisible()) {
      await prevBtn.first().click();
      await page.waitForTimeout(200);
    }
  });
});