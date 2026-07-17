import { TextareaHTMLAttributes, forwardRef } from 'react'

/**
 * Taro <Textarea> 适配 H5 → <textarea>
 * Taro Textarea 事件 onInput 返回 detail.value, detail.cursor
 */
export interface TextareaProps extends Omit<TextareaHTMLAttributes<HTMLTextAreaElement>, 'onInput' | 'onChange' | 'onFocus' | 'onBlur' | 'onConfirm'> {
  value?: string
  placeholder?: string
  maxlength?: number
  autoHeight?: boolean
  fixed?: boolean
  cursorSpacing?: number
  cursor?: number
  selectionStart?: number
  selectionEnd?: number
  adjustPosition?: boolean
  holdKeyboard?: boolean
  disableDefaultPadding?: boolean
  confirmType?: 'send' | 'search' | 'next' | 'go' | 'done'
  confirmHold?: boolean
  showConfirmBar?: boolean
  // 事件
  onInput?: (event: { detail: { value: string; cursor?: number; keyCode?: number } }) => void
  onChange?: (event: { detail: { value: string; cursor?: number } }) => void
  onFocus?: (event: { detail: { value: string; height?: number } }) => void
  onBlur?: (event: { detail: { value: string } }) => void
  onConfirm?: (event: { detail: { value: string } }) => void
  onLinechange?: (event: { detail: { height: number; heightRpx: number; lineCount: number } }) => void
}

export const Textarea = forwardRef<HTMLTextAreaElement, TextareaProps>((props, ref) => {
  const {
    value,
    placeholder,
    maxlength,
    autoHeight,
    fixed,
    cursorSpacing,
    cursor,
    selectionStart,
    selectionEnd,
    adjustPosition,
    holdKeyboard,
    disableDefaultPadding,
    confirmType = 'done',
    confirmHold,
    showConfirmBar,
    onInput,
    onChange,
    onFocus,
    onBlur,
    onConfirm,
    onLinechange,
    style,
    ...rest
  } = props

  return (
    <textarea
      ref={ref}
      placeholder={placeholder}
      maxLength={maxlength}
      defaultValue={rest.defaultValue}
      value={value}
      rows={autoHeight ? 1 : rest.rows || 4}
      style={{
        resize: autoHeight ? 'none' : 'vertical',
        fontFamily: 'inherit',
        ...style
      }}
      onChange={(e) => {
        const val = e.currentTarget.value
        onInput?.({ detail: { value: val, cursor: e.currentTarget.selectionStart } })
        onChange?.({ detail: { value: val, cursor: e.currentTarget.selectionStart } })
      }}
      onFocus={(e) => {
        onFocus?.({ detail: { value: e.currentTarget.value } })
      }}
      onBlur={(e) => {
        onBlur?.({ detail: { value: e.currentTarget.value } })
      }}
      onKeyDown={(e) => {
        if (e.key === 'Enter' && !e.shiftKey) {
          onConfirm?.({ detail: { value: e.currentTarget.value } })
        }
      }}
      {...rest}
    />
  )
})

Textarea.displayName = 'Textarea'
