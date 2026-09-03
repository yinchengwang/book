/**
 * 图解系列 E2E 测试
 *
 * 覆盖按钮（参考 docs/BUTTONS.md Learn Deep L01-L07）
 */
import { test, expect } from '@playwright/test';

const BASE = 'http://localhost:5173';

test.describe('图解系列', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto(BASE + '/learn_deep');
    await page.waitForLoadState('networkidle');
  });

  test('页面加载成功（不报 length 错）', async ({ page }) => {
    await expect(page.locator('.learn-deep-page, .article-list').first()).toBeVisible();
    const errors: string[] = [];
    page.on('pageerror', e => errors.push(e.message));
    await page.waitForTimeout(1000);
    expect(errors.some(e => e.includes('length'))).toBe(false);
  });

  test('分类 chip 渲染', async ({ page }) => {
    const chips = page.locator('.category-chips .chip');
    expect(await chips.count()).toBeGreaterThan(1);
  });

  test('点击分类 chip 过滤', async ({ page }) => {
    const chips = page.locator('.category-chips .chip');
    if (await chips.count() >= 2) {
      await chips.nth(1).click();
      await expect(chips.nth(1)).toHaveClass(/active/);
    }
  });

  test('搜索框输入', async ({ page }) => {
    const search = page.locator('.search-input');
    await search.fill('C++');
    await page.waitForTimeout(300);
  });

  test('点击文章卡进入详情', async ({ page }) => {
    const article = page.locator('.article-card').first();
    if (await article.isVisible()) {
      await article.click();
      await expect(page.locator('.detail-mode, .learn-deep-page.detail-mode').first()).toBeVisible({ timeout: 3000 });
    }
  });

  test('详情页：上一篇/下一篇', async ({ page }) => {
    await page.locator('.article-card').first().click();
    await page.waitForTimeout(500);
    const nextBtn = page.locator('.nav-next');
    if (await nextBtn.isVisible()) {
      await nextBtn.click();
      await page.waitForTimeout(300);
    }
  });

  test('详情页：返回列表', async ({ page }) => {
    await page.locator('.article-card').first().click();
    await page.waitForTimeout(500);
    await page.locator('.back-link, .back-btn').first().click();
    await expect(page.locator('.article-list').first()).toBeVisible();
  });
});