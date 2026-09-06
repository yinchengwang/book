/** @vitest-environment jsdom */
import { describe, it, expect } from 'vitest'
import { render } from '@testing-library/react'
import { GameHeader } from './GameHeader'

describe('GameHeader', () => {
  it('渲染标题和返回按钮', () => {
    const { getByText } = render(<GameHeader title='贪吃蛇' />)
    expect(getByText('贪吃蛇')).toBeTruthy()
  })
})
