/**
 * 摘抄管理 E2E 测试
 *
 * 覆盖按钮（参考 docs/BUTTONS.md Excerpt X01-X16）
 */
import { test, expect } from '@playwright/test';

const BASE = 'http://localhost:5173';

test.describe('摘抄管理', () => {
  test.beforeEach(async ({ page }) => {
    // 注入 v1 老数据，验证 migrate tags 兼容
    await page.goto(BASE + '/');
    await page.evaluate(() => {
      const v1 = {
        state: {
          excerpts: [
            { id: 'old-1', content: '老摘抄', book: '老书', tags: '单字符串' }  // v1 tags 是 string
          ],
          filterBook: '', filterTag: '', filterYear: '', searchQuery: ''
        },
        version: 0
      };
      localStorage.setItem('excerpt-storage', JSON.stringify(v1));
    });
    await page.goto(BASE + '/excerpt');
    await page.waitForLoadState('networkidle');
  });

  test('页面加载成功（v1 老 tags 兼容）', async ({ page }) => {
    await expect(page.locator('.excerpt-page').first()).toBeVisible();
    const errors: string[] = [];
    page.on('pageerror', e => errors.push(e.message));
    await page.waitForTimeout(1000);
    expect(errors.some(e => e.includes('length') || e.includes('includes'))).toBe(false);
  });

  test('摘抄列表渲染', async ({ page }) => {
    const cards = page.locator('.excerpt-card');
    expect(await cards.count()).toBeGreaterThan(0);
  });

  test('搜索框输入', async ({ page }) => {
    const search = page.locator('.search-input');
    await search.fill('指针');
    await page.waitForTimeout(300);
  });

  test('书籍 filter chip 切换', async ({ page }) => {
    const chips = page.locator('.filter-chips .chip');
    if (await chips.count() >= 2) {
      await chips.nth(1).click();
      await expect(chips.nth(1)).toHaveClass(/active/);
    }
  });

  test('点击摘抄卡弹详情', async ({ page }) => {
    const card = page.locator('.excerpt-card').first();
    if (await card.isVisible()) {
      await card.click();
      await expect(page.locator('.modal, .detail-modal').first()).toBeVisible({ timeout: 3000 });
    }
  });

  test('详情：收藏按钮切换', async ({ page }) => {
    const card = page.locator('.excerpt-card').first();
    if (await card.isVisible()) {
      await card.click();
      await page.waitForTimeout(300);
      const fav = page.locator('.favorite-icon').first();
      if (await fav.isVisible()) {
        await fav.click();
        await page.waitForTimeout(200);
      }
    }
  });

  test('添加摘抄表单展开', async ({ page }) => {
    const addBtn = page.locator('.add-btn').first();
    await addBtn.click();
    await expect(page.locator('.add-form, .form').first()).toBeVisible();
  });
});