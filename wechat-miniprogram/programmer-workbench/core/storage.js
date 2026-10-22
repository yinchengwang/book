/**
 * storage.js —— 本地存储封装
 *
 * 前缀 `db:`（新版 schema，不兼容旧 Taro 版的 `devbench:`）。
 * 所有 wx API 调用都在函数内进行，模块加载时零副作用，便于 Node 下单测。
 */

const PREFIX = 'db:'

/** 12 个数据域的存储键 */
const KEYS = {
  todos: 'todos',
  techNotes: 'tech_notes',
  snippets: 'snippets',
  interview: 'interview',
  resources: 'resources',
  reading: 'reading',
  fitness: 'fitness',
  mood: 'mood',
  schedule: 'schedule',
  projects: 'projects',
  insights: 'insights',
  settings: 'settings',
  seeded: '__seeded__'
}

function fullKey (key) {
  return PREFIX + key
}

/**
 * 读取
 * @param {string} key
 * @param {*} defaultValue 读不到时返回
 */
function get (key, defaultValue) {
  try {
    const raw = wx.getStorageSync(fullKey(key))
    if (raw === '' || raw === null || raw === undefined) return defaultValue
    if (typeof raw === 'string') {
      try {
        return JSON.parse(raw)
      } catch (e) {
        return defaultValue
      }
    }
    return raw
  } catch (e) {
    return defaultValue
  }
}

/** 写入 */
function set (key, value) {
  try {
    wx.setStorageSync(fullKey(key), JSON.stringify(value))
    return true
  } catch (e) {
    console.warn('[storage] 写入失败', key, e)
    return false
  }
}

/** 删除单个键 */
function remove (key) {
  try {
    wx.removeStorageSync(fullKey(key))
    return true
  } catch (e) {
    return false
  }
}

/** 清空本应用全部数据 */
function clearAll () {
  try {
    const info = wx.getStorageInfoSync()
    info.keys.forEach(function (k) {
      if (k.indexOf(PREFIX) === 0) wx.removeStorageSync(k)
    })
    return true
  } catch (e) {
    console.warn('[storage] 清空失败', e)
    return false
  }
}

/**
 * 导出全部数据为 JSON 字符串
 * @returns {string} { exportedAt, version, data: { key: value } }
 */
function exportAll () {
  const data = {}
  try {
    const info = wx.getStorageInfoSync()
    info.keys.forEach(function (k) {
      if (k.indexOf(PREFIX) !== 0) return
      const raw = wx.getStorageSync(k)
      if (raw === '' || raw === null || raw === undefined) return
      const shortKey = k.slice(PREFIX.length)
      if (shortKey === KEYS.seeded) return
      try {
        data[shortKey] = typeof raw === 'string' ? JSON.parse(raw) : raw
      } catch (e) {
        data[shortKey] = raw
      }
    })
  } catch (e) {
    console.warn('[storage] 导出失败', e)
  }
  return JSON.stringify({
    exportedAt: new Date().toISOString(),
    version: 1,
    data: data
  }, null, 2)
}

/**
 * 从 JSON 导入
 * @param {string} json
 * @param {'merge'|'overwrite'} mode merge 只填空缺键，overwrite 全量覆盖
 * @returns {{ok:boolean, count:number, error?:string}}
 */
function importAll (json, mode) {
  const m = mode || 'merge'
  try {
    const parsed = JSON.parse(json)
    const data = parsed && parsed.data
    if (!data || typeof data !== 'object') {
      return { ok: false, count: 0, error: '备份格式不正确：缺少 data 字段' }
    }

    let count = 0
    Object.keys(data).forEach(function (k) {
      if (m === 'overwrite') {
        set(k, data[k])
      } else {
        const existing = get(k, null)
        if (existing === null) set(k, data[k])
      }
      count += 1
    })
    return { ok: true, count: count }
  } catch (err) {
    return { ok: false, count: 0, error: (err && err.message) || String(err) }
  }
}

/** 统计占用（KB 估算） */
function usage () {
  try {
    const info = wx.getStorageInfoSync()
    return { keys: info.keys.length, currentSize: info.currentSize || 0 }
  } catch (e) {
    return { keys: 0, currentSize: 0 }
  }
}

module.exports = {
  PREFIX: PREFIX,
  KEYS: KEYS,
  get: get,
  set: set,
  remove: remove,
  clearAll: clearAll,
  exportAll: exportAll,
  importAll: importAll,
  usage: usage
}
