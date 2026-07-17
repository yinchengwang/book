/**
 * 调试 v2 - 找出所有看起来"紫色悬浮"的元素
 * 包括 fixed / absolute / 渐变背景
 */
;(function debugPurple() {
  const all = document.querySelectorAll('*')
  const suspects = []

  all.forEach(el => {
    const style = getComputedStyle(el)
    const rect = el.getBoundingClientRect()
    if (rect.width === 0 || rect.height === 0) return

    // 紫色相关：背景含 #6366f1 / #8b5cf6 / rgb(99,102,241) / rgb(139,92,246)
    const bg = style.background
    const bgImage = style.backgroundImage
    const bgColor = style.backgroundColor

    const isPurple = (s) => s && (
      s.includes('#6366f1') ||
      s.includes('#8b5cf6') ||
      s.includes('#818cf8') ||
      s.includes('#a78bfa') ||
      s.includes('rgb(99, 102, 241)') ||
      s.includes('rgb(139, 92, 246)') ||
      s.includes('rgb(129, 140, 248)') ||
      s.includes('rgb(167, 139, 250)')
    )

    if (isPurple(bg) || isPurple(bgImage) || isPurple(bgColor)) {
      suspects.push({
        tag: el.tagName,
        id: el.id,
        class: el.className?.toString().slice(0, 60),
        position: style.position,
        zIndex: style.zIndex,
        bg: (bgImage || bgColor || bg || '').slice(0, 80),
        rect: { x: rect.x.toFixed(0), y: rect.y.toFixed(0), w: rect.width.toFixed(0), h: rect.height.toFixed(0) },
        text: el.innerText?.slice(0, 30) || ''
      })
    }
  })

  console.clear()
  console.log('%c🔍 当前页面所有紫色元素：', 'font-size:14px; font-weight:bold; color:#a78bfa')
  console.log('='.repeat(80))

  if (suspects.length === 0) {
    console.log('%c没有紫色元素！', 'color:#10b981; font-weight:bold')
  } else {
    console.log(`找到 ${suspects.length} 个紫色相关元素：\n`)
    suspects.forEach((el, i) => {
      console.log(`%c${i + 1}. ${el.tag} #${el.id || '(no id)'} .${el.class}`, 'color:#a78bfa; font-weight:bold')
      console.log(`   position: ${el.position}, z-index: ${el.zIndex}`)
      console.log(`   位置: x=${el.rect.x} y=${el.rect.y}  尺寸: ${el.rect.w}×${el.rect.h}`)
      console.log(`   背景: ${el.bg}`)
      if (el.text) console.log(`   文字: "${el.text}"`)
      console.log('')
    })
  }

  console.log('='.repeat(80))
  console.log('%c复制以上结果发给 Claude', 'color:#818cf8; font-style:italic')

  return suspects
})()
