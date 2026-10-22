// utils/cloud.js — 云服务 SDK 初始化（小程序版，带"未配置"保护）
// 占位 endpoint/publishableKey 时所有 cloud.* 调用直接 reject，
// 页面有兜底处理，不会因为 cloud 没激活而黑屏。

const PUBLIC_CONFIG = {
  endpoint: 'PLACEHOLDER_ENDPOINT',
  publishableKey: 'PLACEHOLDER_PUBLISHABLE_KEY'
}

function checkConfigured() {
  return PUBLIC_CONFIG.endpoint && PUBLIC_CONFIG.endpoint !== 'PLACEHOLDER_ENDPOINT'
    && PUBLIC_CONFIG.publishableKey && PUBLIC_CONFIG.publishableKey !== 'PLACEHOLDER_PUBLISHABLE_KEY'
}

// 延迟 require：仅在 ensureCloud 时才加载 SDK，避免 module-load 期副作用
let _sdkMod = null
function loadSDK() {
  if (_sdkMod) return _sdkMod
  _sdkMod = require('@tencent-ai/workbuddy-cloud-sdk/miniprogram')
  return _sdkMod
}

let _cloud = null
let _pending = null

async function doInit() {
  if (_cloud) return _cloud
  const sdk = loadSDK()
  const c = sdk.createMiniProgramWorkBuddyCloud({
    endpoint: PUBLIC_CONFIG.endpoint,
    publishableKey: PUBLIC_CONFIG.publishableKey
  })
  _cloud = c
  return c
}

function ensureCloud() {
  if (!checkConfigured()) {
    return Promise.reject(new Error('[cloud] not configured — wait for activation'))
  }
  if (_cloud) return Promise.resolve(_cloud)
  if (_pending) return _pending
  _pending = doInit().finally(() => { _pending = null })
  return _pending
}

async function getCloud() {
  if (!checkConfigured()) {
    throw new Error('[cloud] not configured — wait for activation')
  }
  if (_cloud) return _cloud
  return await ensureCloud()
}

module.exports = {
  ensureCloud,
  getCloud,
  isConfigured: checkConfigured,
  get PUBLIC_CONFIG() { return PUBLIC_CONFIG }
}
