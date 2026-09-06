import { test, expect } from '@playwright/test'

test.describe('authentication', () => {
  test('redirects to /login when unauthenticated', async ({ page }) => {
    await page.goto('/')
    await expect(page).toHaveURL(/\/login/)
  })

  test('login form rejects empty fields', async ({ page }) => {
    await page.goto('/login')
    await expect(page.locator('[data-test="submit"]')).toBeDisabled()
  })

  test('user can navigate to register', async ({ page }) => {
    await page.goto('/login')
    await page.click('text=立即注册')
    await expect(page).toHaveURL(/\/register/)
  })
})
