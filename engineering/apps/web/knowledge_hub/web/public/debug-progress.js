/**
 * 调试 v4 - 看 progress-bar 父容器和 progress-fill 子元素的真实尺寸
 */
;(function checkProgress() {
  const bars = document.querySelectorAll('.progress-bar')
  const fills = document.querySelectorAll('.progress-fill')

  console.clear()
  console.log('%c🔍 进度条结构检查：', 'font-size:14px; font-weight:bold; color:#818cf8')
  console.log('='.repeat(80))

  console.log(`\n.progress-bar 数量: ${bars.length}`)
  bars.forEach((bar, i) => {
    const rect = bar.getBoundingClientRect()
    const style = getComputedStyle(bar)
    console.log(`  ${i + 1}. 位置(${rect.x.toFixed(0)},${rect.y.toFixed(0)}) 尺寸 ${rect.width.toFixed(0)}×${rect.height.toFixed(0)} height=${style.height}`)
  })

  console.log(`\n.progress-fill 数量: ${fills.length}`)
  fills.forEach((fill, i) => {
    const rect = fill.getBoundingClientRect()
    const style = getComputedStyle(fill)
    console.log(`  ${i + 1}. 位置(${rect.x.toFixed(0)},${rect.y.toFixed(0)}) 尺寸 ${rect.width.toFixed(0)}×${rect.height.toFixed(0)} height=${style.height}`)
    console.log(`     父元素: ${fill.parentElement?.className || '(none)'} size ${fill.parentElement?.getBoundingClientRect().height.toFixed(0)}px`)
  })

  // 检查 .dashboard 容器
  const dash = document.querySelector('.dashboard')
  if (dash) {
    const rect = dash.getBoundingClientRect()
    const style = getComputedStyle(dash)
    console.log(`\n.dashboard: 位置(${rect.x},${rect.y}) 尺寸 ${rect.width}×${rect.height} min-height=${style.minHeight}`)
  }

  // 检查 .page-container
  const page = document.querySelector('.page-container')
  if (page) {
    const rect = page.getBoundingClientRect()
    console.log(`.page-container: 尺寸 ${rect.width}×${rect.height}`)
  }

  // 检查 .domains-list
  const list = document.querySelector('.domains-list')
  if (list) {
    const rect = list.getBoundingClientRect()
    const style = getComputedStyle(list)
    console.log(`.domains-list: 尺寸 ${rect.width}×${rect.height} flex-direction=${style.flexDirection}`)
  }

  // 检查 .domain-item
  const item = document.querySelector('.domain-item')
  if (item) {
    const rect = item.getBoundingClientRect()
    const style = getComputedStyle(item)
    console.log(`.domain-item: 尺寸 ${rect.width}×${rect.height} display=${style.display} flex-direction=${style.flexDirection}`)
  }

  console.log('='.repeat(80))
})()
