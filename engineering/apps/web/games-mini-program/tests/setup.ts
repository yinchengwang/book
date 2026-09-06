import { vi } from 'vitest'
import React from 'react'

vi.mock('@tarojs/components', () => ({
  View: (props: any) => React.createElement('div', props),
  Text: (props: any) => React.createElement('span', props),
  Button: (props: any) => React.createElement('button', props),
  Picker: (props: any) => React.createElement('select', props)
}))

vi.mock('@tarojs/taro', () => ({
  default: {
    getStorageSync: vi.fn(() => undefined),
    setStorageSync: vi.fn(),
    removeStorageSync: vi.fn(),
    navigateBack: vi.fn(),
    vibrateShort: vi.fn(),
    showToast: vi.fn(),
    showShareMenu: vi.fn()
  }
}))
