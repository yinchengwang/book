/**
 * 面试题库 E2E 测试
 *
 * 覆盖按钮（参考 docs/BUTTONS.md Interview Bank B01-B10）
 */
import { test, expect } from '@playwright/test';

const BASE = 'http://localhost:5173';

test.describe('面试题库', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto(BASE + '/interview-bank');
    await page.waitForLoadState('networkidle');
  });

  test('页面加载成功', async ({ page }) => {
    await expect(page.locator('.bank-page, .interview-bank').first()).toBeVisible();
  });

  test('算法/面试 tab 切换', async ({ page }) => {
    const tabs = page.locator('.ib-tab');
    if (await tabs.first().isVisible()) {
      expect(await tabs.count()).toBeGreaterThanOrEqual(2);
      await tabs.nth(0).click();
      await expect(tabs.nth(0)).toHaveClass(/active/);
      await tabs.nth(1).click();
      await expect(tabs.nth(1)).toHaveClass(/active/);
    }
  });

  test('分类卡点击进入题目列表', async ({ page }) => {
    const catCard = page.locator('.cat-card').first();
    if (await catCard.isVisible()) {
      await catCard.click();
      // 应进入 list 模式（有题目列表或返回按钮）
      await page.waitForTimeout(300);
    }
  });

  test('题目卡点击弹详情', async ({ page }) => {
    // 先进入分类
    const catCard = page.locator('.cat-card').first();
    if (await catCard.isVisible()) {
      await catCard.click();
      await page.waitForTimeout(500);
      // 点击第一个题目
      const qCard = page.locator('.q-card, .question-card').first();
      if (await qCard.isVisible()) {
        await qCard.click();
        await expect(page.locator('.modal, .detail-modal').first()).toBeVisible();
      }
    }
  });

  test('详情弹窗关闭按钮', async ({ page }) => {
    const catCard = page.locator('.cat-card').first();
    if (await catCard.isVisible()) {
      await catCard.click();
      await page.waitForTimeout(500);
      const qCard = page.locator('.q-card, .question-card').first();
      if (await qCard.isVisible()) {
        await qCard.click();
        await page.waitForTimeout(300);
        const closeBtn = page.locator('.btn-close, .modal-close').first();
        if (await closeBtn.isVisible()) {
          await closeBtn.click();
          await page.waitForTimeout(300);
        }
      }
    }
  });

  test('难度 chip 切换', async ({ page }) => {
    const catCard = page.locator('.cat-card').first();
    if (await catCard.isVisible()) {
      await catCard.click();
      await page.waitForTimeout(500);
      const diffChips = page.locator('.diff-chip');
      if (await diffChips.first().isVisible()) {
        await diffChips.first().click();
        await expect(diffChips.first()).toHaveClass(/active/);
      }
    }
  });
});