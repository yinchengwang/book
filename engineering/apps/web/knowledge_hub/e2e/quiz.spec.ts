/**
 * 题库练习 E2E 测试
 *
 * 覆盖按钮（参考 docs/BUTTONS.md Quiz Q01-Q29）
 */
import { test, expect } from '@playwright/test';

const BASE = 'http://localhost:5173';

test.describe('题库练习', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto(BASE + '/quiz');
    await page.waitForLoadState('networkidle');
  });

  test('页面加载成功（plan 模式）', async ({ page }) => {
    await expect(page.locator('.quiz-page')).toBeVisible();
    await expect(page.locator('.plan-card')).toBeVisible();
  });

  test('plan 模式 4 个统计项', async ({ page }) => {
    const stats = page.locator('.plan-mode .stats-row .stat-item');
    expect(await stats.count()).toBe(4);
  });

  test('点击"开始今日计划"进入答题模式', async ({ page }) => {
    await page.locator('.btn-primary:has-text("开始今日计划")').click();
    await expect(page.locator('.quiz-mode')).toBeVisible({ timeout: 5000 });
  });

  test('点击"换一批"刷新今日计划', async ({ page }) => {
    await page.locator('.btn-secondary:has-text("换一批")').click();
    await page.waitForTimeout(300);
  });

  test('点击"浏览题库"entry 进入 list 模式', async ({ page }) => {
    await page.locator('.entry-card:has-text("浏览题库")').click();
    await expect(page.locator('.list-mode')).toBeVisible();
  });

  test('点击"错题本"entry', async ({ page }) => {
    await page.locator('.entry-card:has-text("错题本")').click();
    await expect(page.locator('.wrong-mode')).toBeVisible();
  });

  test('点击"学习统计"entry', async ({ page }) => {
    await page.locator('.entry-card:has-text("学习统计")').click();
    await expect(page.locator('.stats-mode')).toBeVisible();
  });

  test('list 模式：点击 stack chip 过滤', async ({ page }) => {
    await page.locator('.entry-card:has-text("浏览题库")').click();
    await page.locator('.chip:has-text("CPP")').click();
    await expect(page.locator('.chip.active:has-text("CPP")')).toBeVisible();
  });

  test('list 模式：顶部 3 统计显示', async ({ page }) => {
    await page.locator('.entry-card:has-text("浏览题库")').click();
    const stats = page.locator('.list-stats .list-stat');
    expect(await stats.count()).toBe(3);
  });

  test('list 模式：折叠卡点击展开', async ({ page }) => {
    await page.locator('.entry-card:has-text("浏览题库")').click();
    await page.waitForTimeout(500);
    const firstCard = page.locator('.question-list-item').first();
    if (await firstCard.isVisible()) {
      await firstCard.locator('.item-summary').click();
      await expect(firstCard).toHaveClass(/--expanded/);
    }
  });

  test('list 模式：返回计划按钮', async ({ page }) => {
    await page.locator('.entry-card:has-text("浏览题库")').click();
    await page.locator('.back-btn').click();
    await expect(page.locator('.plan-mode')).toBeVisible();
  });

  test('quiz 模式：答题后提交', async ({ page }) => {
    await page.locator('.btn-primary:has-text("开始今日计划")').click();
    await page.waitForTimeout(500);
    // 点击第一个选项
    const firstOption = page.locator('.option').first();
    if (await firstOption.isVisible()) {
      await firstOption.click();
      // 提交
      await page.locator('.btn-primary:has-text("提交答案")').click();
      await expect(page.locator('.result-feedback')).toBeVisible({ timeout: 3000 });
    }
  });

  test('quiz 模式：✕ 退出按钮', async ({ page }) => {
    await page.locator('.btn-primary:has-text("开始今日计划")').click();
    await page.locator('.exit-btn').click();
    await expect(page.locator('.plan-mode')).toBeVisible();
  });

  test('7 个 stack 进度卡点击进入 list 模式', async ({ page }) => {
    const stackCards = page.locator('.stack-grid .stack-card');
    expect(await stackCards.count()).toBe(7);
    await stackCards.first().click();
    await expect(page.locator('.list-mode')).toBeVisible();
  });
});