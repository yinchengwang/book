import { HTMLAttributes, ReactNode, forwardRef, useState, useEffect } from 'react'

/**
 * Taro <Switch> 适配 H5 → 自定义开关组件
 * 兼容 checked / onChange / color / disabled 等属性
 */
export interface SwitchProps extends Omit<HTMLAttributes<HTMLDivElement>, 'onChange'> {
  checked?: boolean
  defaultChecked?: boolean
  disabled?: boolean
  color?: string
  type?: 'switch' | 'checkbox'
  onChange?: (event: { detail: { value: boolean } }) => void
}

export const Switch = forwardRef<HTMLDivElement, SwitchProps>((props, ref) => {
  const {
    checked,
    defaultChecked = false,
    disabled,
    color = '#6366f1',
    type = 'switch',
    onChange,
    style,
    ...rest
  } = props

  // controlled vs uncontrolled
  const isControlled = checked !== undefined
  const [innerChecked, setInnerChecked] = useState(defaultChecked)
  const currentChecked = isControlled ? checked : innerChecked

  const handleClick = () => {
    if (disabled) return
    const newValue = !currentChecked
    if (!isControlled) {
      setInnerChecked(newValue)
    }
    onChange?.({ detail: { value: newValue } })
  }

  return (
    <div
      ref={ref}
      role="switch"
      aria-checked={currentChecked}
      onClick={handleClick}
      style={{
        display: 'inline-block',
        width: type === 'switch' ? 44 : 18,
        height: type === 'switch' ? 26 : 18,
        background: currentChecked ? color : 'rgba(255,255,255,0.12)',
        borderRadius: type === 'switch' ? 13 : 3,
        position: 'relative',
        cursor: disabled ? 'not-allowed' : 'pointer',
        opacity: disabled ? 0.5 : 1,
        transition: 'background 0.2s',
        verticalAlign: 'middle',
        userSelect: 'none',
        ...style
      }}
      {...rest}
    >
      <div
        style={{
          position: 'absolute',
          top: type === 'switch' ? 3 : 0,
          left: type === 'switch' ? (currentChecked ? 21 : 3) : 0,
          width: type === 'switch' ? 20 : 18,
          height: type === 'switch' ? 20 : 18,
          background: '#fff',
          borderRadius: type === 'switch' ? '50%' : 3,
          transition: 'left 0.2s',
          boxShadow: '0 2px 4px rgba(0,0,0,0.2)'
        }}
      />
    </div>
  )
})

Switch.displayName = 'Switch'
