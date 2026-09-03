import { ButtonHTMLAttributes, ReactNode, forwardRef } from 'react'

/**
 * Taro <Button> 适配 H5 → <button>
 * Taro 中 Button 有 openType（小程序专属）如 share、feedback
 * H5 端忽略 openType，保持原生 button 行为
 */
export type ButtonType = 'primary' | 'default' | 'warn'
export type ButtonSize = 'default' | 'mini'

export interface ButtonProps extends Omit<ButtonHTMLAttributes<HTMLButtonElement>, 'type'> {
  size?: ButtonSize
  type?: ButtonType
  plain?: boolean
  disabled?: boolean
  loading?: boolean
  // Taro 小程序专属，H5 端忽略
  openType?: 'share' | 'feedback' | 'contact' | 'getPhoneNumber' | 'getUserInfo'
  formType?: 'submit' | 'reset'
  hoverClass?: string
  hoverStopPropagation?: boolean
  hoverStartTime?: number
  hoverStayTime?: number
  lang?: string
  sessionFrom?: string
  sendMessageTitle?: string
  sendMessagePath?: string
  sendMessageImg?: string
  showMessageCard?: boolean
  appParameter?: string
  children?: ReactNode
}

export const Button = forwardRef<HTMLButtonElement, ButtonProps>((props, ref) => {
  const {
    size = 'default',
    type = 'default',
    plain,
    disabled,
    loading,
    openType,
    formType,
    hoverClass,
    hoverStopPropagation,
    hoverStartTime,
    hoverStayTime,
    lang,
    sessionFrom,
    sendMessageTitle,
    sendMessagePath,
    sendMessageImg,
    showMessageCard,
    appParameter,
    style,
    children,
    ...rest
  } = props

  return (
    <button
      ref={ref}
      type={formType || 'button'}
      disabled={disabled || loading}
      data-taro-button={openType}
      style={{
        cursor: disabled ? 'not-allowed' : 'pointer',
        opacity: disabled ? 0.5 : 1,
        padding: size === 'mini' ? '4px 12px' : '8px 16px',
        fontSize: size === 'mini' ? '12px' : '14px',
        ...style
      }}
      {...rest}
    >
      {loading && <span style={{ marginRight: 6 }}>⏳</span>}
      {children}
    </button>
  )
})

Button.displayName = 'Button'
