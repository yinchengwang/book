/**
 * 首页仪表盘 E2E 测试
 *
 * 覆盖按钮（参考 docs/BUTTONS.md 仪表盘）：
 * - D01 打卡 chip 显示
 * - D02 查看详细计划
 * - D03-D07 5 个总览 stat-card
 * - D08-D14 7 个技术栈进度卡
 * - D15-D16 高估盲点 + 评测系统链接
 * - D17-D21 5 年成长路线
 * - D22 查看完整计划链接
 */
import { test, expect } from '@playwright/test';

const BASE = 'http://localhost:5173';

test.describe('首页仪表盘', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto(BASE + '/');
    await page.waitForLoadState('networkidle');
  });

  test('页面加载成功', async ({ page }) => {
    await expect(page.locator('.greeting')).toContainText('仪表盘');
  });

  test('五年计划当前进程卡渲染', async ({ page }) => {
    const planCard = page.locator('.plan-card');
    await expect(planCard).toBeVisible();
    // 8 个今日打卡 chip
    const chips = page.locator('.plan-chip');
    expect(await chips.count()).toBeGreaterThanOrEqual(1);
  });

  test('查看详细计划链接可点击', async ({ page }) => {
    const link = page.locator('.plan-link').first();
    await expect(link).toBeVisible();
    await link.click();
    // 跳转到 /five_year_plan
    await expect(page).toHaveURL(/five_year_plan/);
  });

  test('5 个总览 stat-card 显示', async ({ page }) => {
    const statCards = page.locator('.overview-grid .stat-card');
    expect(await statCards.count()).toBe(5);
  });

  test('知识点总数 / 已测 / 平均分 / 已掌握 / 累计次数 stat-card', async ({ page }) => {
    const labels = ['知识点总数', '已测', '平均测验', '已掌握', '累计测验'];
    for (const label of labels) {
      await expect(page.locator(`.stat-card:has-text("${label}")`).first()).toBeVisible();
    }
  });

  test('7 个技术栈进度卡可点击跳转到 /quiz', async ({ page }) => {
    const stackCards = page.locator('.stack-card');
    expect(await stackCards.count()).toBe(7);
    // 点击第一个 stack 卡
    await stackCards.first().click();
    await expect(page).toHaveURL(/\/quiz/);
  });

  test('高估盲点项点击跳转到 /quiz', async ({ page }) => {
    const blindItem = page.locator('.blind-item').first();
    if (await blindItem.isVisible()) {
      await blindItem.click();
      await expect(page).toHaveURL(/\/quiz/);
    } else {
      // 无盲点时空状态正常
      await expect(page.locator('.empty').first()).toBeVisible();
    }
  });

  test('5 年成长路线渲染 5 个 stage', async ({ page }) => {
    const stages = page.locator('.roadmap-stage');
    expect(await stages.count()).toBe(5);
  });

  test('roadmap-stage 可点击跳转到 /five_year_plan', async ({ page }) => {
    const stage = page.locator('.roadmap-stage').first();
    await stage.click();
    await expect(page).toHaveURL(/five_year_plan/);
  });

  test('查看完整计划链接跳转', async ({ page }) => {
    const link = page.locator('.roadmap-link');
    await link.click();
    await expect(page).toHaveURL(/five_year_plan/);
  });

  test('评测系统链接（空状态）跳转', async ({ page }) => {
    const link = page.locator('text=评测系统');
    if (await link.first().isVisible()) {
      await link.first().click();
      await expect(page).toHaveURL(/\/quiz/);
    }
  });
});