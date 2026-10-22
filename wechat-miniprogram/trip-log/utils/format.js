// utils/format.js — 日期、字符串格式化

function pad2(n) { return n < 10 ? '0' + n : '' + n }

function todayISO() {
  const d = new Date()
  return d.getFullYear() + '-' + pad2(d.getMonth() + 1) + '-' + pad2(d.getDate())
}

function ymdPretty(iso) {
  if (!iso) return ''
  const [y, m, d] = iso.split('-')
  return y + '年' + parseInt(m, 10) + '月' + parseInt(d, 10) + '日'
}

function ymdShort(iso) {
  if (!iso) return ''
  const [y, m, d] = iso.split('-')
  return y + '.' + parseInt(m, 10) + '.' + parseInt(d, 10)
}

function groupKey(iso) {
  if (!iso) return '未知时间'
  const [y, m] = iso.split('-')
  return y + '-' + m
}

function groupLabel(iso) {
  if (!iso) return '未知时间'
  const [y, m] = iso.split('-')
  return y + '年' + parseInt(m, 10) + '月'
}

module.exports = {
  pad2, todayISO, ymdPretty, ymdShort, groupKey, groupLabel
}
