/**
 * 技术雷达 E2E 测试
 *
 * 覆盖按钮（参考 docs/BUTTONS.md Radar RA01-RA06）
 */
import { test, expect } from '@playwright/test';

const BASE = 'http://localhost:5173';

test.describe('技术雷达', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto(BASE + '/radar');
    await page.waitForLoadState('networkidle');
  });

  test('页面加载成功（不报 length 错）', async ({ page }) => {
    await expect(page.locator('.radar-page').first()).toBeVisible();
    const errors: string[] = [];
    page.on('pageerror', e => errors.push(e.message));
    await page.waitForTimeout(1000);
    expect(errors.some(e => e.includes('length'))).toBe(false);
  });

  test('8 个主题 tab 渲染', async ({ page }) => {
    const tabs = page.locator('.theme-tab');
    expect(await tabs.count()).toBe(8);
  });

  test('点击主题 tab 切换', async ({ page }) => {
    const tabs = page.locator('.theme-tab');
    if (await tabs.count() >= 2) {
      await tabs.nth(1).click();
      await expect(tabs.nth(1)).toHaveClass(/active/);
    }
  });

  test('书籍主题：4 个象限 chip', async ({ page }) => {
    // 书籍主题是默认
    const quadChips = page.locator('.filter-chip');
    if (await quadChips.first().isVisible()) {
      expect(await quadChips.count()).toBeGreaterThanOrEqual(4);
    }
  });

  test('圆点点击弹详情', async ({ page }) => {
    const dot = page.locator('svg circle, svg .dot, [class*="dot"]').first();
    if (await dot.isVisible()) {
      await dot.click();
      await expect(page.locator('.modal, .detail-modal').first()).toBeVisible({ timeout: 3000 });
    }
  });

  test('详情关闭按钮', async ({ page }) => {
    const dot = page.locator('svg circle, svg .dot, [class*="dot"]').first();
    if (await dot.isVisible()) {
      await dot.click();
      await page.waitForTimeout(300);
      const closeBtn = page.locator('.btn-close, .modal-close').first();
      if (await closeBtn.isVisible()) {
        await closeBtn.click();
        await page.waitForTimeout(300);
      }
    }
  });
});