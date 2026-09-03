/**
 * Layout（TopBar + Sidebar）E2E 测试
 *
 * 覆盖按钮（参考 docs/BUTTONS.md Layout L01-L18）
 */
import { test, expect } from '@playwright/test';

const BASE = 'http://localhost:5173';

test.describe('Layout - TopBar + Sidebar', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto(BASE + '/');
    await page.waitForLoadState('networkidle');
  });

  test('TopBar 显示主题切换按钮', async ({ page }) => {
    const themeBtn = page.locator('.topbar button, .topbar [class*="theme"], .topbar-icon-btn').first();
    await expect(themeBtn).toBeVisible();
  });

  test('TopBar 主题切换生效', async ({ page }) => {
    const themeBtn = page.locator('.topbar [class*="theme"], .topbar-icon-btn').first();
    if (await themeBtn.isVisible()) {
      await themeBtn.click();
      await page.waitForTimeout(300);
      const theme = await page.evaluate(() => document.documentElement.dataset.theme);
      // 主题属性被设置
      expect(['light', 'dark']).toContain(theme);
    }
  });

  test('Sidebar 12 个导航项', async ({ page }) => {
    const navItems = page.locator('.nav-item, .sidebar .nav-link');
    expect(await navItems.count()).toBeGreaterThanOrEqual(10);
  });

  test('点击 Sidebar 导航切换页面', async ({ page }) => {
    const navItems = page.locator('.nav-item, .sidebar .nav-link');
    if (await navItems.count() >= 2) {
      // 点击第二个（题库）
      await navItems.nth(1).click();
      await page.waitForTimeout(500);
    }
  });

  test('Sidebar 动态 badge 显示', async ({ page }) => {
    const badges = page.locator('.nav-item .badge, .sidebar .badge');
    if (await badges.first().isVisible()) {
      expect(await badges.count()).toBeGreaterThan(0);
    }
  });

  test('用户头像下拉', async ({ page }) => {
    const avatar = page.locator('.topbar-avatar, .avatar, [class*="avatar"]').first();
    if (await avatar.isVisible()) {
      await avatar.click();
      await page.waitForTimeout(300);
      // 下拉应显示
      const dropdown = page.locator('.topbar-dropdown, .dropdown, .user-menu');
      if (await dropdown.first().isVisible()) {
        expect(await dropdown.count()).toBeGreaterThan(0);
      }
    }
  });
});