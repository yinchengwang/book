/**
 * 调试 - 找出页面里所有的 fixed 元素
 * 在 Console 运行
 */
(function debugFixed() {
  const all = document.querySelectorAll('*')
  const fixed = []
  all.forEach(el => {
    const style = getComputedStyle(el)
    if (style.position === 'fixed') {
      const rect = el.getBoundingClientRect()
      if (rect.width > 0 && rect.height > 0) {
        fixed.push({
          tag: el.tagName,
          id: el.id,
          class: el.className?.toString().slice(0, 60),
          bg: style.background?.slice(0, 60),
          zIndex: style.zIndex,
          position: { x: rect.x, y: rect.y, w: rect.width, h: rect.height },
          text: el.innerText?.slice(0, 40) || ''
        })
      }
    }
  })

  console.clear()
  console.log('%c🔍 当前页面所有 fixed 元素：', 'font-size:14px; font-weight:bold; color:#818cf8')
  console.log('='.repeat(80))

  if (fixed.length === 0) {
    console.log('%c没有 fixed 元素！', 'color:#10b981; font-weight:bold')
  } else {
    fixed.forEach((el, i) => {
      console.log(`\n%c${i + 1}. ${el.tag} #${el.id || '(no id)'}`, 'color:#a78bfa; font-weight:bold')
      console.log(`   class: ${el.class}`)
      console.log(`   背景: ${el.bg}`)
      console.log(`   位置: x=${el.position.x.toFixed(0)} y=${el.position.y.toFixed(0)}  尺寸: ${el.position.w.toFixed(0)}×${el.position.h.toFixed(0)}`)
      console.log(`   z-index: ${el.zIndex}`)
      if (el.text) console.log(`   文字: "${el.text}"`)
    })
  }

  console.log('\n' + '='.repeat(80))
  console.log('%c复制以上结果发给 Claude', 'color:#818cf8; font-style:italic')

  return fixed
})()
