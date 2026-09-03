import { ImgHTMLAttributes, forwardRef } from 'react'

/**
 * Taro <Image> 适配 H5 → <img>
 * Taro 中 Image 有 mode（裁剪模式）: scaleToFill, aspectFit, aspectFill, widthFix, heightFix, top, bottom, center, left, right, top left, top right, bottom left, bottom right
 * H5 端用 CSS object-fit 模拟
 */
export type ImageMode =
  | 'scaleToFill'
  | 'aspectFit'
  | 'aspectFill'
  | 'widthFix'
  | 'heightFix'
  | 'top'
  | 'bottom'
  | 'center'
  | 'left'
  | 'right'
  | 'top left'
  | 'top right'
  | 'bottom left'
  | 'bottom right'

export interface ImageProps extends Omit<ImgHTMLAttributes<HTMLImageElement>, 'onLoad' | 'onError'> {
  src: string
  mode?: ImageMode
  lazyLoad?: boolean
  showMenuByLongpress?: boolean
  onLoad?: (event: { detail: { width: number; height: number } }) => void
  onError?: (event: { detail: { errMsg: string } }) => void
}

const modeToObjectFit: Record<ImageMode, string> = {
  scaleToFill: 'fill',
  aspectFit: 'contain',
  aspectFill: 'cover',
  widthFix: 'none',  // 高度按比例
  heightFix: 'none', // 宽度按比例
  top: 'cover',
  bottom: 'cover',
  center: 'cover',
  left: 'cover',
  right: 'cover',
  'top left': 'cover',
  'top right': 'cover',
  'bottom left': 'cover',
  'bottom right': 'cover'
}

const modeToObjectPosition: Partial<Record<ImageMode, string>> = {
  top: 'top',
  bottom: 'bottom',
  center: 'center',
  left: 'left',
  right: 'right',
  'top left': 'top left',
  'top right': 'top right',
  'bottom left': 'bottom left',
  'bottom right': 'bottom right'
}

export const Image = forwardRef<HTMLImageElement, ImageProps>((props, ref) => {
  const {
    src,
    mode = 'scaleToFill',
    lazyLoad,
    showMenuByLongpress,
    onLoad,
    onError,
    style,
    ...rest
  } = props

  const objectFit = modeToObjectFit[mode]
  const objectPosition = modeToObjectPosition[mode] || 'center'

  return (
    <img
      ref={ref}
      src={src}
      loading={lazyLoad ? 'lazy' : 'eager'}
      onLoad={(e) => {
        const img = e.currentTarget
        onLoad?.({ detail: { width: img.naturalWidth, height: img.naturalHeight } })
      }}
      onError={(e) => {
        onError?.({ detail: { errMsg: 'image load failed' } })
      }}
      style={{
        objectFit: objectFit as any,
        objectPosition,
        // 阻止长按菜单
        WebkitTouchCallout: showMenuByLongpress ? 'default' : 'none',
        userSelect: 'none',
        ...style
      }}
      {...rest}
    />
  )
})

Image.displayName = 'Image'
