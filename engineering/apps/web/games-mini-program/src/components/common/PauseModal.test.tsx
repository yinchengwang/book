/** @vitest-environment jsdom */
import { describe, it, expect, vi } from 'vitest'
import { render } from '@testing-library/react'
import { PauseModal } from './PauseModal'

describe('PauseModal', () => {
  it('暂停时渲染操作按钮', () => {
    const { getByText } = render(
      <PauseModal visible onResume={vi.fn()} onQuit={vi.fn()} onRestart={vi.fn()} />
    )
    expect(getByText('已暂停')).toBeTruthy()
    expect(getByText('继续')).toBeTruthy()
  })
})
