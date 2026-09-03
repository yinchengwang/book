/**
 * 知识图谱 E2E 测试
 *
 * 覆盖按钮（参考 docs/BUTTONS.md Knowledge Graph K01-K07）
 */
import { test, expect } from '@playwright/test';

const BASE = 'http://localhost:5173';

test.describe('知识图谱', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto(BASE + '/knowledge_graph');
    await page.waitForLoadState('networkidle');
  });

  test('页面加载成功', async ({ page }) => {
    await expect(page.locator('.knowledge-page, .knowledge-graph').first()).toBeVisible();
  });

  test('SVG 节点渲染', async ({ page }) => {
    const nodes = page.locator('svg circle, svg rect, svg .node, [data-node-id]');
    expect(await nodes.count()).toBeGreaterThan(0);
  });

  test('点击节点弹详情', async ({ page }) => {
    const node = page.locator('svg circle, svg rect, svg .node').first();
    if (await node.isVisible()) {
      await node.click();
      await expect(page.locator('.modal, .node-modal, .detail-modal').first()).toBeVisible({ timeout: 3000 });
    }
  });

  test('详情弹窗关闭', async ({ page }) => {
    const node = page.locator('svg circle, svg rect, svg .node').first();
    if (await node.isVisible()) {
      await node.click();
      await page.waitForTimeout(300);
      const closeBtn = page.locator('.modal-close, .btn-close, .close-btn').first();
      if (await closeBtn.isVisible()) {
        await closeBtn.click();
        await page.waitForTimeout(300);
      }
    }
  });

  test('掌握度 +/- 按钮', async ({ page }) => {
    const node = page.locator('svg circle, svg rect, svg .node').first();
    if (await node.isVisible()) {
      await node.click();
      await page.waitForTimeout(300);
      const minusBtn = page.locator('.level-btn:has-text("−"), .level-btn:has-text("-")').first();
      if (await minusBtn.isVisible()) {
        await minusBtn.click();
        await page.waitForTimeout(200);
      }
      const plusBtn = page.locator('.level-btn:has-text("+")').first();
      if (await plusBtn.isVisible()) {
        await plusBtn.click();
        await page.waitForTimeout(200);
      }
    }
  });
});