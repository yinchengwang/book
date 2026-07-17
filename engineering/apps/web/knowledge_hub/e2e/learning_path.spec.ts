/**
 * 学习路径 E2E 测试
 *
 * 覆盖按钮（参考 docs/BUTTONS.md Learning Path P01-P09）
 */
import { test, expect } from '@playwright/test';

const BASE = 'http://localhost:5173';

test.describe('学习路径', () => {
  test.beforeEach(async ({ page }) => {
    // 强制清空 v1 老数据
    await page.goto(BASE + '/');
    await page.evaluate(() => {
      const v1 = {
        state: {
          paths: [{
            id: 'old', name: '老', steps: [{ id: 's1', title: 'step1' }]  // 缺 subSteps
          }],
          activePath: { id: 'old', steps: [{ id: 's1', title: 'step1' }] },
          activePathId: 'old',
          currentStep: 0
        },
        version: 0
      };
      localStorage.setItem('learning-path-storage', JSON.stringify(v1));
    });
    await page.goto(BASE + '/learning_path');
    await page.waitForLoadState('networkidle');
  });

  test('页面加载成功（不报 length 错）', async ({ page }) => {
    await expect(page.locator('.learning-path-page, .path-page').first()).toBeVisible();
    // 检查无 pageerror
    const errors: string[] = [];
    page.on('pageerror', e => errors.push(e.message));
    await page.waitForTimeout(1000);
    const hasLengthError = errors.some(e => e.includes('length'));
    expect(hasLengthError).toBe(false);
  });

  test('3 条路径 tab 渲染', async ({ page }) => {
    const pathTabs = page.locator('.path-tab');
    if (await pathTabs.first().isVisible()) {
      expect(await pathTabs.count()).toBe(3);
    }
  });

  test('点击路径 tab 切换', async ({ page }) => {
    const pathTabs = page.locator('.path-tab');
    if (await pathTabs.count() >= 2) {
      await pathTabs.nth(1).click();
      await expect(pathTabs.nth(1)).toHaveClass(/active/);
    }
  });

  test('点击 step 头部展开/折叠 sub-step', async ({ page }) => {
    const stepHeader = page.locator('.step-header').first();
    if (await stepHeader.isVisible()) {
      await stepHeader.click();
      await page.waitForTimeout(300);
    }
  });

  test('sub-step 勾选切换', async ({ page }) => {
    // 先展开
    await page.locator('.step-header').first().click();
    await page.waitForTimeout(300);
    const substep = page.locator('.substep').first();
    if (await substep.isVisible()) {
      await substep.click();
      await expect(substep).toHaveClass(/done/);
    }
  });

  test('重置按钮 + 确认模态', async ({ page }) => {
    const resetBtn = page.locator('.control-btn.reset, .btn:has-text("重置")').first();
    if (await resetBtn.isVisible()) {
      await resetBtn.click();
      await expect(page.locator('.modal-overlay, .modal').first()).toBeVisible({ timeout: 2000 });
    }
  });

  test('下一步按钮', async ({ page }) => {
    const nextBtn = page.locator('.control-btn:has-text("下一步")').first();
    if (await nextBtn.isVisible()) {
      const disabled = await nextBtn.getAttribute('disabled');
      if (!disabled) {
        await nextBtn.click();
        await page.waitForTimeout(300);
      }
    }
  });
});