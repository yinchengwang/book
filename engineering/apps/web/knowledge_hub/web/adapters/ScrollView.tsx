import { HTMLAttributes, ReactNode, forwardRef, useRef, useEffect, useState, useCallback } from 'react'

/**
 * Taro <ScrollView> 适配 H5 → <div style="overflow:auto">
 * Taro ScrollView 有 scrollX/scrollY 方向、scrollIntoView、scrollTop、onScroll 等
 * H5 端用 CSS overflow + scrollTo API 模拟
 */
export interface ScrollViewProps extends HTMLAttributes<HTMLDivElement> {
  scrollX?: boolean
  scrollY?: boolean
  upperThreshold?: number
  lowerThreshold?: number
  scrollTop?: number
  scrollLeft?: number
  scrollIntoView?: string
  scrollWithAnimation?: boolean
  enableBackToTop?: boolean
  showScrollbar?: boolean
  enhanced?: boolean
  bounces?: boolean
  decelerationRate?: number
  // 事件
  onScrollToUpper?: (event: { detail: { direction: 'top' | 'left' } }) => void
  onScrollToLower?: (event: { detail: { direction: 'bottom' | 'right' } }) => void
  onScroll?: (event: { detail: { scrollLeft: number; scrollTop: number; scrollHeight: number; scrollWidth: number; deltaX: number; deltaY: number } }) => void
  children?: ReactNode
}

export const ScrollView = forwardRef<HTMLDivElement, ScrollViewProps>((props, ref) => {
  const {
    scrollX = false,
    scrollY = true,
    upperThreshold = 50,
    lowerThreshold = 50,
    scrollTop,
    scrollLeft,
    scrollIntoView,
    scrollWithAnimation = false,
    enableBackToTop,
    showScrollbar = true,
    enhanced,
    bounces,
    decelerationRate,
    onScrollToUpper,
    onScrollToLower,
    onScroll,
    style,
    children,
    ...rest
  } = props

  const innerRef = useRef<HTMLDivElement>(null)
  const lastScrollTop = useRef(0)
  const lastScrollLeft = useRef(0)

  // 处理 scrollTop / scrollLeft 控制
  useEffect(() => {
    if (!innerRef.current) return
    if (scrollTop !== undefined) {
      innerRef.current.scrollTop = scrollTop
    }
    if (scrollLeft !== undefined) {
      innerRef.current.scrollLeft = scrollLeft
    }
  }, [scrollTop, scrollLeft])

  // 处理 scrollIntoView
  useEffect(() => {
    if (!scrollIntoView || !innerRef.current) return
    const el = innerRef.current.querySelector(`#${scrollIntoView}`) as HTMLElement
    el?.scrollIntoView({ behavior: scrollWithAnimation ? 'smooth' : 'auto' })
  }, [scrollIntoView, scrollWithAnimation])

  const handleScroll = useCallback((e: React.UIEvent<HTMLDivElement>) => {
    const target = e.currentTarget
    const deltaX = target.scrollLeft - lastScrollLeft.current
    const deltaY = target.scrollTop - lastScrollTop.current

    onScroll?.({
      detail: {
        scrollLeft: target.scrollLeft,
        scrollTop: target.scrollTop,
        scrollHeight: target.scrollHeight,
        scrollWidth: target.scrollWidth,
        deltaX,
        deltaY
      }
    })

    // 触顶
    if (scrollY && target.scrollTop <= upperThreshold) {
      onScrollToUpper?.({ detail: { direction: 'top' } })
    }
    // 触底
    if (scrollY && target.scrollHeight - target.scrollTop - target.clientHeight <= lowerThreshold) {
      onScrollToLower?.({ detail: { direction: 'bottom' } })
    }
    // 触左
    if (scrollX && target.scrollLeft <= upperThreshold) {
      onScrollToUpper?.({ detail: { direction: 'left' } })
    }
    // 触右
    if (scrollX && target.scrollWidth - target.scrollLeft - target.clientWidth <= lowerThreshold) {
      onScrollToLower?.({ detail: { direction: 'right' } })
    }

    lastScrollTop.current = target.scrollTop
    lastScrollLeft.current = target.scrollLeft
  }, [onScroll, onScrollToUpper, onScrollToLower, scrollX, scrollY, upperThreshold, lowerThreshold])

  return (
    <div
      ref={ref}
      onScroll={handleScroll}
      style={{
        overflowX: scrollX ? 'auto' : 'hidden',
        overflowY: scrollY ? 'auto' : 'hidden',
        WebkitOverflowScrolling: 'touch',
        ...(showScrollbar ? {} : {
          scrollbarWidth: 'none',
          msOverflowStyle: 'none'
        }),
        ...style
      }}
      {...rest}
    >
      <div ref={innerRef} style={{ display: 'inline-block', minWidth: '100%' }}>
        {children}
      </div>
    </div>
  )
})

ScrollView.displayName = 'ScrollView'
