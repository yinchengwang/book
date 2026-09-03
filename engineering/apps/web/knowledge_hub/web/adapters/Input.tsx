import { InputHTMLAttributes, forwardRef } from 'react'

/**
 * Taro <Input> 适配 H5 → <input>
 * Taro Input 事件 onInput 返回 detail.value
 * H5 端做事件格式转换
 */
export type InputType = 'text' | 'number' | 'idcard' | 'digit' | 'password' | 'tel' | 'email' | 'url'

export interface InputProps extends Omit<InputHTMLAttributes<HTMLInputElement>, 'onInput' | 'onChange' | 'onFocus' | 'onBlur' | 'onConfirm'> {
  type?: InputType
  password?: boolean
  placeholder?: string
  placeholderStyle?: string  // H5 端忽略
  placeholderClass?: string
  maxlength?: number
  cursorSpacing?: number
  autoFocus?: boolean
  focus?: boolean
  confirmType?: 'send' | 'search' | 'next' | 'go' | 'done'
  confirmHold?: boolean
  cursor?: number
  selectionStart?: number
  selectionEnd?: number
  adjustPosition?: boolean
  holdKeyboard?: boolean
  // 事件 - Taro 格式
  onInput?: (event: { detail: { value: string; cursor?: number; keyCode?: number } }) => void
  onChange?: (event: { detail: { value: string; cursor?: number } }) => void
  onFocus?: (event: { detail: { value: string; height?: number } }) => void
  onBlur?: (event: { detail: { value: string } }) => void
  onConfirm?: (event: { detail: { value: string } }) => void
}

export const Input = forwardRef<HTMLInputElement, InputProps>((props, ref) => {
  const {
    type = 'text',
    password,
    placeholder,
    placeholderStyle,
    placeholderClass,
    maxlength,
    cursorSpacing,
    autoFocus,
    focus,
    confirmType = 'done',
    confirmHold,
    cursor,
    selectionStart,
    selectionEnd,
    adjustPosition,
    holdKeyboard,
    onInput,
    onChange,
    onFocus,
    onBlur,
    onConfirm,
    ...rest
  } = props

  return (
    <input
      ref={ref}
      type={password ? 'password' : type}
      placeholder={placeholder}
      maxLength={maxlength}
      autoFocus={autoFocus || focus}
      defaultValue={rest.defaultValue}
      value={rest.value}
      onChange={(e) => {
        const value = e.currentTarget.value
        onInput?.({ detail: { value, cursor: e.currentTarget.selectionStart || 0 } })
        onChange?.({ detail: { value, cursor: e.currentTarget.selectionStart || 0 } })
      }}
      onFocus={(e) => {
        onFocus?.({ detail: { value: e.currentTarget.value } })
      }}
      onBlur={(e) => {
        onBlur?.({ detail: { value: e.currentTarget.value } })
      }}
      onKeyDown={(e) => {
        if (e.key === 'Enter') {
          onConfirm?.({ detail: { value: e.currentTarget.value } })
        }
      }}
      {...rest}
    />
  )
})

Input.displayName = 'Input'
