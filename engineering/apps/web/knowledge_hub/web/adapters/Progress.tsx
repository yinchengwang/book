import { HTMLAttributes, ReactNode, forwardRef } from 'react'

/**
 * Taro <Progress> 适配 H5 → 自定义进度条
 * 兼容 percent / strokeWidth / color / activeColor / backgroundColor / showInfo / active / duration
 */
export interface ProgressProps extends HTMLAttributes<HTMLDivElement> {
  percent?: number
  strokeWidth?: number
  color?: string
  activeColor?: string | { from: string; to: string }
  backgroundColor?: string
  showInfo?: boolean
  active?: boolean
  duration?: number
  borderRadius?: number | string
  fontSize?: number
  children?: ReactNode
}

export const Progress = forwardRef<HTMLDivElement, ProgressProps>((props, ref) => {
  const {
    percent = 0,
    strokeWidth = 16,
    color,
    activeColor = '#6366f1',
    backgroundColor = 'rgba(255,255,255,0.1)',
    showInfo = false,
    active = false,
    duration = 30,
    borderRadius,
    fontSize = 14,
    style,
    children,
    ...rest
  } = props

  // 计算背景渐变
  let bgImage = ''
  if (typeof activeColor === 'object' && activeColor.from && activeColor.to) {
    bgImage = `linear-gradient(90deg, ${activeColor.from}, ${activeColor.to})`
  } else if (typeof activeColor === 'string') {
    bgImage = `linear-gradient(90deg, ${activeColor}, ${activeColor})`
  }
  if (color) {
    bgImage = `linear-gradient(90deg, ${color}, ${color})`
  }

  // 圆角
  const br = borderRadius !== undefined
    ? (typeof borderRadius === 'number' ? `${borderRadius}px` : borderRadius)
    : `${strokeWidth / 2}px`

  // 动画
  const animationStyle = active
    ? {
        backgroundSize: '200% 100%',
        animation: `progress-active ${duration * 1000}ms ease infinite`,
        backgroundImage: typeof activeColor === 'object'
          ? `linear-gradient(90deg, ${activeColor.from}, ${activeColor.to}, ${activeColor.from})`
          : `linear-gradient(90deg, ${activeColor}, ${activeColor})`
      }
    : {}

  return (
    <div
      ref={ref}
      style={{
        display: 'inline-flex',
        alignItems: 'center',
        gap: 8,
        width: '100%',
        ...style
      }}
      {...rest}
    >
      <div
        style={{
          flex: 1,
          height: strokeWidth,
          background: backgroundColor,
          borderRadius: br,
          overflow: 'hidden',
          position: 'relative'
        }}
      >
        <div
          style={{
            position: 'absolute',
            top: 0,
            left: 0,
            height: '100%',
            width: `${Math.min(100, Math.max(0, percent))}%`,
            backgroundImage: bgImage,
            borderRadius: br,
            transition: 'width 0.3s ease',
            ...animationStyle
          }}
        />
      </div>
      {showInfo && (
        <span
          style={{
            fontSize,
            color: '#fff',
            minWidth: 36,
            textAlign: 'right',
            flexShrink: 0
          }}
        >
          {percent}%
        </span>
      )}
      {children}
    </div>
  )
})

Progress.displayName = 'Progress'
