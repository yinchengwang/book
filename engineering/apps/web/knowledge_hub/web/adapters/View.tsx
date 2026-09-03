import { HTMLAttributes, ReactNode, forwardRef, useRef, useEffect } from 'react'

/**
 * Taro <View> 适配 H5 → <div>
 * Taro 专属属性: hoverClass, hoverStopPropagation, hoverStartTime, hoverStayTime
 * H5 端忽略这些，全部用 CSS :hover 实现
 */
export interface ViewProps extends Omit<HTMLAttributes<HTMLDivElement>, 'onTouchStart' | 'onTouchMove' | 'onTouchEnd' | 'onTouchCancel'> {
  hoverClass?: string
  hoverStopPropagation?: boolean
  hoverStartTime?: number
  hoverStayTime?: number
  catchMove?: boolean
  animation?: Record<string, any>
  // Taro 专属 touch 事件
  onTouchStart?: (e?: any) => void
  onTouchMove?: (e?: any) => void
  onTouchEnd?: (e?: any) => void
  onTouchCancel?: (e?: any) => void
  onLongTap?: (e?: any) => void
  children?: ReactNode
}

export const View = forwardRef<HTMLDivElement, ViewProps>((props, ref) => {
  const {
    hoverClass,
    hoverStopPropagation,
    hoverStartTime,
    hoverStayTime,
    catchMove,
    animation,
    onTouchStart,
    onTouchMove,
    onTouchEnd,
    onTouchCancel,
    onLongTap,
    style,
    children,
    ...rest
  } = props

  // 长按 timer
  const longTapTimer = useRef<any>(null)

  useEffect(() => {
    return () => {
      if (longTapTimer.current) {
        clearTimeout(longTapTimer.current)
        longTapTimer.current = null
      }
    }
  }, [])

  // catchMove 在 H5 端处理 touch 事件
  const touchHandlers = catchMove ? {
    onTouchStart: (e: React.TouchEvent) => {
      e.preventDefault()
      onTouchStart?.(e)
    },
    onTouchMove: (e: React.TouchEvent) => {
      e.preventDefault()
      onTouchMove?.(e)
    }
  } : {
    onTouchStart,
    onTouchMove,
    onTouchEnd,
    onTouchCancel
  }

  // 长按事件用 mouse down + timer 模拟
  const handleMouseDown = onLongTap ? () => {
    longTapTimer.current = setTimeout(() => {
      onLongTap()
      longTapTimer.current = null
    }, 500)
  } : undefined

  const clearLongTap = () => {
    if (longTapTimer.current) {
      clearTimeout(longTapTimer.current)
      longTapTimer.current = null
    }
  }

  return (
    <div
      ref={ref}
      style={style}
      {...rest}
      onClick={(e) => {
        if (typeof (rest as any).onClick === 'function') {
          ;(rest as any).onClick(e)
        }
      }}
      onMouseDown={onLongTap ? () => {
        longTapTimer.current = setTimeout(() => {
          onLongTap()
          longTapTimer.current = null
        }, 500)
      } : rest.onMouseDown}
      onMouseUp={onLongTap ? () => {
        if (longTapTimer.current) {
          clearTimeout(longTapTimer.current)
          longTapTimer.current = null
        }
      } : undefined}
      onMouseLeave={onLongTap ? () => {
        if (longTapTimer.current) {
          clearTimeout(longTapTimer.current)
          longTapTimer.current = null
        }
      } : undefined}
    >
      {children}
    </div>
  )
})

View.displayName = 'View'
