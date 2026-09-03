/**
 * 调试 v3 - 看 splash 是否还在
 */
(function checkSplash() {
  const splash = document.getElementById('splash')
  const splashLogo = document.getElementById('splash-logo')

  console.clear()
  console.log('%c🔍 Splash 状态：', 'font-size:14px; font-weight:bold; color:#818cf8')
  console.log('='.repeat(80))

  if (!splash) {
    console.log('%c✅ splash 元素已被移除', 'color:#10b981; font-weight:bold')
  } else {
    console.log('%c❌ splash 还在！', 'color:#ef4444; font-weight:bold')
    console.log('   display:', splash.style.display)
    console.log('   visibility:', getComputedStyle(splash).visibility)
    console.log('   opacity:', getComputedStyle(splash).opacity)
    console.log('   position:', getComputedStyle(splash).position)
    console.log('   z-index:', getComputedStyle(splash).zIndex)
    console.log('   背景:', getComputedStyle(splash).background)
    console.log('   pointer-events:', getComputedStyle(splash).pointerEvents)
    const rect = splash.getBoundingClientRect()
    console.log('   位置/尺寸:', `${rect.x},${rect.y} ${rect.width}×${rect.height}`)
  }

  // 检查进度条
  const fills = document.querySelectorAll('.progress-fill')
  console.log(`\n紫色进度条: ${fills.length} 个`)
  if (fills.length > 0) {
    const first = fills[0]
    const rect = first.getBoundingClientRect()
    console.log(`第一个: 位置 y=${rect.y.toFixed(0)} 尺寸 ${rect.width.toFixed(0)}×${rect.height.toFixed(0)}`)
  }

  // 检查 Dashboard 的实际内容
  const main = document.querySelector('.page-container')
  const mainText = main?.innerText?.slice(0, 200) || '(空)'
  console.log(`\n主内容区文本 (前200字):`)
  console.log(mainText)

  console.log('='.repeat(80))
})()
