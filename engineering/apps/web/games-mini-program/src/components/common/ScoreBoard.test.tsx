/** @vitest-environment jsdom */
import { describe, it, expect } from 'vitest'
import { render } from '@testing-library/react'
import { ScoreBoard } from './ScoreBoard'

describe('ScoreBoard', () => {
  it('渲染当前得分和最高分', () => {
    const { getByText } = render(<ScoreBoard score={42} best={100} />)
    expect(getByText('42')).toBeTruthy()
    expect(getByText('100')).toBeTruthy()
  })
})
