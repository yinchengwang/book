import { test, expect } from '@playwright/test'

test.describe('todo CRUD', () => {
  test.beforeEach(async ({ page }) => {
    // Stub auth so the guard allows access
    await page.addInitScript(() => {
      window.localStorage.setItem(
        'auth',
        JSON.stringify({
          user: { id: 1, email: 'a@b.com', username: 'alice', role: 'editor', created_at: 0 },
          token: { access_token: 'test-token', expires_at: Date.now() + 3600_000 }
        })
      )
    })
  })

  test('list view renders', async ({ page }) => {
    await page.goto('/')
    await expect(page.locator('.filter-bar')).toBeVisible()
  })

  test('inline edit opens input on double click', async ({ page }) => {
    await page.goto('/')
    const card = page.locator('[data-id]').first()
    if (await card.count() === 0) test.skip()
    await card.locator('.todo-title').dblclick()
    await expect(page.locator('[data-test="inline-edit-input"]')).toBeVisible()
  })
})
