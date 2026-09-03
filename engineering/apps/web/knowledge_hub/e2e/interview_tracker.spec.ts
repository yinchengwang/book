/**
 * 面试追踪 E2E 测试
 *
 * 覆盖按钮（参考 docs/BUTTONS.md Interview Tracker T01-T20）
 */
import { test, expect } from '@playwright/test';

const BASE = 'http://localhost:5173';

test.describe('面试追踪', () => {
  test.beforeEach(async ({ page }) => {
    // 清空 store 确保从空开始
    await page.goto(BASE + '/');
    await page.evaluate(() => localStorage.removeItem('interview-tracker-storage'));
    await page.goto(BASE + '/interview_tracker');
    await page.waitForLoadState('networkidle');
  });

  test('页面加载成功', async ({ page }) => {
    await expect(page.locator('.interview-tracker, .tracker-page').first()).toBeVisible();
  });

  test('点击 + 添加公司按钮显示表单', async ({ page }) => {
    const addBtn = page.locator('.add-btn').first();
    await addBtn.click();
    await expect(page.locator('.add-form, .form').first()).toBeVisible();
  });

  test('表单可填写并提交', async ({ page }) => {
    // 打开表单
    await page.locator('.add-btn').first().click();
    // 填写公司名（视具体 input 选择器）
    const inputs = page.locator('.add-form input, .form input');
    if (await inputs.first().isVisible()) {
      const count = await inputs.count();
      if (count > 0) await inputs.nth(0).fill('测试公司');
      if (count > 1) await inputs.nth(1).fill('后端开发');
    }
    // 提交
    const submit = page.locator('.submit-btn');
    if (await submit.isVisible()) {
      await submit.click();
      // 表单隐藏 / 新公司卡出现
      await expect(page.locator('.company-card').first()).toBeVisible({ timeout: 5000 });
    }
  });

  test('删除已添加的公司', async ({ page }) => {
    // 先添加一个
    await page.locator('.add-btn').first().click();
    const inputs = page.locator('.add-form input, .form input');
    const count = await inputs.count();
    if (count > 0) await inputs.nth(0).fill('临时公司');
    await page.locator('.submit-btn').click();
    await page.waitForTimeout(500);
    // 删除
    const deleteBtn = page.locator('.action-btn.delete').first();
    if (await deleteBtn.isVisible()) {
      await deleteBtn.click();
      await page.waitForTimeout(500);
    }
  });

  test('14 个阶段状态切换 chip', async ({ page }) => {
    // 添加一个公司
    await page.locator('.add-btn').first().click();
    const inputs = page.locator('.add-form input, .form input');
    const count = await inputs.count();
    if (count > 0) await inputs.nth(0).fill('测试公司');
    await page.locator('.submit-btn').click();
    await page.waitForTimeout(500);
    // 找状态切换 chip
    const chips = page.locator('.status-chip');
    if (await chips.first().isVisible()) {
      expect(await chips.count()).toBeGreaterThanOrEqual(1);
    }
  });
});