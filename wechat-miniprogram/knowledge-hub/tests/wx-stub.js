/**
 * wx-stub.js —— Node 环境下模拟 wx API
 *
 * 内置内存版 storage（getStorageSync / setStorageSync / removeStorageSync /
 * getStorageInfoSync），使 store 与 storage 模块的单测能跑真实读写路径。
 * 未知 API 一律返回 no-op，避免页面模块顶层调用 wx 时报错。
 */

const memory = new Map()

function install () {
  const noop = function () {}

  const base = {
    getStorageSync: function (key) {
      return memory.has(key) ? memory.get(key) : ''
    },
    setStorageSync: function (key, value) {
      memory.set(key, value)
    },
    removeStorageSync: function (key) {
      memory.delete(key)
    },
    clearStorageSync: function () {
      memory.clear()
    },
    getStorageInfoSync: function () {
      const keys = Array.from(memory.keys())
      let size = 0
      memory.forEach(function (v) {
        size += String(v).length
      })
      return { keys: keys, currentSize: Math.ceil(size / 1024), limitSize: 10240 }
    },
    getSystemInfoSync: function () {
      return {
        theme: 'light',
        pixelRatio: 2,
        platform: 'devtools',
        windowWidth: 375,
        windowHeight: 667
      }
    },
    getWindowInfo: function () {
      return { pixelRatio: 2, windowWidth: 375, windowHeight: 667 }
    },
    setNavigationBarColor: noop,
    setTabBarStyle: noop,
    onThemeChange: noop,
    offThemeChange: noop,
    vibrateShort: noop,
    vibrateLong: noop,
    createSelectorQuery: function () {
      return {
        select: function () {
          return {
            fields: function () {
              return { exec: noop }
            },
            boundingClientRect: function () {
              return { exec: noop }
            }
          }
        },
        selectAll: function () {
          return {
            fields: function () {
              return { exec: noop }
            }
          }
        }
      }
    }
  }

  global.wx = new Proxy(base, {
    get: function (target, prop) {
      if (prop in target) return target[prop]
      return noop
    }
  })

  global.getApp = function () { return { globalData: {} } }
  global.getCurrentPages = function () { return [] }
}

/** 清空内存存储（单测每个用例前调用） */
function reset () {
  memory.clear()
}

/** 直接读内存，便于断言 */
function raw (key) {
  return memory.has(key) ? memory.get(key) : undefined
}

module.exports = {
  install: install,
  reset: reset,
  raw: raw,
  memory: memory
}
