/**
 * 复习计划 E2E 测试
 *
 * 覆盖按钮（参考 docs/BUTTONS.md Review）：
 * - R01-R04 4 个统计卡可点击切 tab
 * - R05-R07 3 个 tab 切换
 * - R08-R13 6 个评分按钮
 * - R14 继续下一轮
 */
import { test, expect } from '@playwright/test';

const BASE = 'http://localhost:5173';

test.describe('复习计划', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto(BASE + '/review');
    await page.waitForLoadState('networkidle');
  });

  test('页面加载成功', async ({ page }) => {
    await expect(page.locator('.review-page')).toBeVisible();
  });

  test('4 个统计卡渲染', async ({ page }) => {
    const cards = page.locator('.review-stats .stat-card');
    expect(await cards.count()).toBe(4);
  });

  test('点击"待复习"统计卡激活并切到对应 tab', async ({ page }) => {
    const card = page.locator('.stat-card.stat-due');
    await card.click();
    await expect(card).toHaveClass(/active/);
    await expect(page.locator('.tab.active:has-text("待复习")')).toBeVisible();
  });

  test('点击"7天内"统计卡切到全部计划 tab', async ({ page }) => {
    const card = page.locator('.stat-card.stat-upcoming');
    await card.click();
    await expect(page.locator('.tab.active:has-text("全部计划")')).toBeVisible();
  });

  test('点击"已延期"统计卡切到全部计划 tab', async ({ page }) => {
    const card = page.locator('.stat-card.stat-overdue');
    await card.click();
    await expect(page.locator('.tab.active:has-text("全部计划")')).toBeVisible();
  });

  test('点击"已掌握"统计卡切到已完成 tab', async ({ page }) => {
    const card = page.locator('.stat-card.stat-done');
    await card.click();
    await expect(page.locator('.tab.active:has-text("已完成")')).toBeVisible();
  });

  test('3 个 tab 切换', async ({ page }) => {
    const tabs = page.locator('.review-tabs .tab');
    expect(await tabs.count()).toBe(3);
    // 全部计划
    await tabs.nth(1).click();
    await expect(tabs.nth(1)).toHaveClass(/active/);
    // 已完成
    await tabs.nth(2).click();
    await expect(tabs.nth(2)).toHaveClass(/active/);
    // 回到待复习
    await tabs.nth(0).click();
    await expect(tabs.nth(0)).toHaveClass(/active/);
  });

  test('复习评分按钮：当前题存在时显示 6 个评分', async ({ page }) => {
    const ratingButtons = page.locator('.rating-btn');
    if (await ratingButtons.first().isVisible()) {
      expect(await ratingButtons.count()).toBe(6);
    } else {
      // 无待复习时显示空状态
      await expect(page.locator('.review-empty')).toBeVisible();
    }
  });
});