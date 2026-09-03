// src/hooks/useSafeArea.ts
// 安全区域 Hook

export const useSafeArea = () => {
  // #ifdef H5
  // H5 端返回 0，实际使用 CSS env()
  return {
    top: 0,
    bottom: 0,
    left: 0,
    right: 0,
    topEnv: 'env(safe-area-inset-top)',
    bottomEnv: 'env(safe-area-inset-bottom)'
  }
  // #endif

  // #ifdef MP-WEIXIN
  // 小程序使用 wx.getSystemInfoSync
  try {
    const systemInfo = wx.getSystemInfoSync()
    const safeArea = systemInfo.safeArea || { top: 0, bottom: 0, left: 0, right: 0 }
    return {
      top: safeArea.top || 0,
      bottom: (systemInfo.screenHeight || 0) - safeArea.bottom,
      left: safeArea.left || 0,
      right: (systemInfo.screenWidth || 0) - safeArea.right,
      topEnv: `${safeArea.top || 0}px`,
      bottomEnv: `${(systemInfo.screenHeight || 0) - safeArea.bottom}px`
    }
  } catch {
    return {
      top: 0,
      bottom: 0,
      left: 0,
      right: 0,
      topEnv: '0px',
      bottomEnv: '0px'
    }
  }
  // #endif

  return {
    top: 0,
    bottom: 0,
    left: 0,
    right: 0,
    topEnv: '0px',
    bottomEnv: '0px'
  }
}
