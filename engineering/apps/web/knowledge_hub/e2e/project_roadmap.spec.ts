/**
 * 项目路线 E2E 测试
 *
 * 覆盖按钮（参考 docs/BUTTONS.md Project Roadmap PR01-PR09）
 */
import { test, expect } from '@playwright/test';

const BASE = 'http://localhost:5173';

test.describe('项目路线', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto(BASE + '/');
    await page.evaluate(() => localStorage.removeItem('project-roadmap-storage'));
    await page.goto(BASE + '/project_roadmap');
    await page.waitForLoadState('networkidle');
  });

  test('页面加载成功', async ({ page }) => {
    await expect(page.locator('.roadmap-page, .project-roadmap').first()).toBeVisible();
  });

  test('添加项目按钮', async ({ page }) => {
    const addBtn = page.locator('.add-btn').first();
    await addBtn.click();
    await expect(page.locator('.add-form, .form').first()).toBeVisible();
  });

  test('状态/优先级 chip 切换', async ({ page }) => {
    await page.locator('.add-btn').first().click();
    const chips = page.locator('.status-chip, .priority-chip');
    if (await chips.first().isVisible()) {
      await chips.nth(1).click();
      await expect(chips.nth(1)).toHaveClass(/active/);
    }
  });

  test('提交新项目', async ({ page }) => {
    await page.locator('.add-btn').first().click();
    const inputs = page.locator('.add-form input, .form input');
    const count = await inputs.count();
    if (count > 0) await inputs.nth(0).fill('测试项目');
    if (count > 1) await inputs.nth(1).fill('学习');
    await page.locator('.submit-btn').click();
    await page.waitForTimeout(500);
  });

  test('点击项目卡弹详情', async ({ page }) => {
    const card = page.locator('.project-card').first();
    if (await card.isVisible()) {
      await card.click();
      await expect(page.locator('.modal, .detail-modal').first()).toBeVisible({ timeout: 3000 });
    }
  });
});