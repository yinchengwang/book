// src/hooks/usePlatform.ts
// 平台检测 Hook

export const usePlatform = () => {
  // #ifdef H5
  return {
    isH5: true,
    isMP: false,
    isRN: false,
    platform: 'h5' as const
  }
  // #endif

  // #ifdef MP-WEIXIN
  return {
    isH5: false,
    isMP: true,
    isRN: false,
    platform: 'weapp' as const
  }
  // #endif

  // #ifdef RN
  return {
    isH5: false,
    isMP: false,
    isRN: true,
    platform: 'rn' as const
  }
  // #endif

  return {
    isH5: true,
    isMP: false,
    isRN: false,
    platform: 'unknown' as const
  }
}
