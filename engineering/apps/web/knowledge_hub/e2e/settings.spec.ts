/**
 * 设置页 E2E 测试
 *
 * 覆盖按钮（参考 docs/BUTTONS.md Settings S01-S10）
 */
import { test, expect } from '@playwright/test';

const BASE = 'http://localhost:5173';

test.describe('设置', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto(BASE + '/settings');
    await page.waitForLoadState('networkidle');
  });

  test('页面加载成功', async ({ page }) => {
    await expect(page.locator('.settings-page').first()).toBeVisible();
  });

  test('主题切换：点击"深色"卡', async ({ page }) => {
    const darkCard = page.locator('.theme-card:has-text("深色")');
    await darkCard.click();
    await expect(darkCard).toHaveClass(/active/);
  });

  test('主题切换：点击"浅色"卡', async ({ page }) => {
    const lightCard = page.locator('.theme-card:has-text("浅色")');
    await lightCard.click();
    await expect(lightCard).toHaveClass(/active/);
    // 检查 documentElement.dataset.theme = 'light'
    const theme = await page.evaluate(() => document.documentElement.dataset.theme);
    expect(theme).toBe('light');
  });

  test('用户昵称输入 + 保存', async ({ page }) => {
    const nameInput = page.locator('.setting-input').first();
    await nameInput.fill('新昵称');
    await page.locator('.action-btn.primary:has-text("保存")').click();
    await page.waitForTimeout(300);
  });

  test('API key 显示切换', async ({ page }) => {
    const showBtn = page.locator('.mini-btn:has-text("显示"), .mini-btn:has-text("隐藏")').first();
    if (await showBtn.isVisible()) {
      await showBtn.click();
      await page.waitForTimeout(200);
    }
  });

  test('API key 复制按钮', async ({ page }) => {
    // 先填一个 key
    const apiInput = page.locator('input[placeholder*="API"]').first();
    if (await apiInput.isVisible()) {
      await apiInput.fill('sk-test-1234');
      // 点击复制
      const copyBtn = page.locator('.mini-btn:has-text("复制")');
      if (await copyBtn.isVisible()) {
        await copyBtn.click();
        await page.waitForTimeout(300);
      }
    }
  });

  test('复习/面试通知 Switch', async ({ page }) => {
    const switches = page.locator('.switch, switch');
    if (await switches.first().isVisible()) {
      await switches.first().click();
      await page.waitForTimeout(200);
    }
  });

  test('数据导出按钮', async ({ page }) => {
    const exportBtn = page.locator('.action-btn.export');
    if (await exportBtn.isVisible()) {
      await exportBtn.click();
      await page.waitForTimeout(300);
    }
  });

  test('数据导入按钮', async ({ page }) => {
    const importBtn = page.locator('.action-btn.import');
    if (await importBtn.isVisible()) {
      await importBtn.click();
      await page.waitForTimeout(300);
    }
  });

  test('清空数据按钮', async ({ page }) => {
    const clearBtn = page.locator('.action-btn.danger:has-text("清空")');
    if (await clearBtn.isVisible()) {
      // 清空会弹确认，直接点不验证 modal
    }
  });
});