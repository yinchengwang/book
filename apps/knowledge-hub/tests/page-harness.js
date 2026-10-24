/**
 * page-harness.js —— 页面级测试脚手架
 *
 * 加载真实页面模块，构造一个可直接调用生命周期的实例：
 *   - 复制 Page 配置上的方法与 data
 *   - 提供 setData 实现（浅合并，与小程序行为一致）
 *
 * 用法：
 *   const h = require('./page-harness.js')
 *   const page = h.createPage('pages/dashboard/index')
 *   page.onLoad()
 *   h.tap(page, 'onToggleTodo', { id: 'x' })
 */

const path = require('path')

/** 加载页面并返回可调用实例 */
function createPage (route) {
  const abs = path.resolve(__dirname, '..', route + '.js')
  let config = null
  const prevPage = global.Page
  global.Page = function (o) { config = o }

  try {
    delete require.cache[require.resolve(abs)]
    require(abs)
  } finally {
    global.Page = prevPage
  }

  if (!config) throw new Error('页面未调用 Page(): ' + route)

  const inst = {}
  Object.keys(config).forEach(function (k) { inst[k] = config[k] })
  inst.data = JSON.parse(JSON.stringify(config.data))
  // 支持 'a.b' 形式的 setData 路径（与小程序一致），未知层自动补对象
  inst.setData = function (patch) {
    Object.keys(patch).forEach(function (k) {
      if (k.indexOf('.') < 0) {
        inst.data[k] = patch[k]
        return
      }
      const parts = k.split('.')
      let node = inst.data
      for (let i = 0; i < parts.length - 1; i++) {
        if (typeof node[parts[i]] !== 'object' || node[parts[i]] === null) node[parts[i]] = {}
        node = node[parts[i]]
      }
      node[parts[parts.length - 1]] = patch[k]
    })
  }
  return inst
}

/** 模拟 bindinput / bindchange 事件 */
function fire (page, handler, value) {
  page[handler]({ detail: { value: value } })
}

/** 模拟 bindtap 事件（带 dataset） */
function tap (page, handler, dataset) {
  page[handler]({ currentTarget: { dataset: dataset || {} } })
}

/**
 * 加载 Component 并返回可调用实例
 *
 * 提供 data / setData / triggerEvent 与 methods 上的全部方法，
 * 事件记录在 __events 里便于断言。
 */
function createComponent (route) {
  const abs = path.resolve(__dirname, '..', route + '.js')
  let config = null
  const prev = global.Component
  global.Component = function (o) { config = o }

  try {
    delete require.cache[require.resolve(abs)]
    require(abs)
  } finally {
    global.Component = prev
  }

  if (!config) throw new Error('组件未调用 Component(): ' + route)

  const inst = {
    data: {},
    __events: [],
    setData: function (patch) {
      Object.keys(patch).forEach(function (k) { inst.data[k] = patch[k] })
    },
    triggerEvent: function (name, detail) {
      inst.__events.push({ name: name, detail: detail })
    }
  }

  // 属性默认值
  Object.keys(config.properties || {}).forEach(function (k) {
    inst.data[k] = config.properties[k].value
  })

  // 方法
  Object.keys(config.methods || {}).forEach(function (k) {
    inst[k] = config.methods[k]
  })

  return inst
}

/** 取某个事件的最后一次触发 */
function lastEvent (inst, name) {
  for (let i = inst.__events.length - 1; i >= 0; i--) {
    if (inst.__events[i].name === name) return inst.__events[i]
  }
  return null
}

/** 临时替换某个 wx API，返回还原函数 */
function mockWx (name, impl) {
  const original = wx[name]
  wx[name] = impl
  return function () { wx[name] = original }
}

module.exports = {
  createPage: createPage,
  createComponent: createComponent,
  lastEvent: lastEvent,
  fire: fire,
  tap: tap,
  mockWx: mockWx
}
