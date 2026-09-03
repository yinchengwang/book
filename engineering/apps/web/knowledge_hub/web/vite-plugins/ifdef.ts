/**
 * Vite 插件 - 简化版 Taro 条件编译
 *
 * 同时支持 Vite transform 和 esbuild 插件
 * 源码中 #ifdef MP-WEIXIN ... #endif 包裹的代码块在 H5 端被移除
 * #ifdef H5 ... #endif 包裹的代码块在 H5 端被保留
 */

function processIfDef(code: string): string {
  // 处理 #ifdef H5 / #ifdef MP-WEIXIN 指令
  let result = code

  // 匹配 #ifdef MP-WEIXIN ... #endif 块
  // 支持 // 注释 和 /* */ 块注释
  const ifDefRegex = /\/\/\s*#ifdef\s+(MP-WEIXIN|MP-ALIPAY|MP-TT|MP-QQ|MP-SWAN|MP-JD)\s*\n([\s\S]*?)\/\/\s*#endif/gm
  result = result.replace(ifDefRegex, () => '')

  // 移除 #ifndef H5 块（在 H5 端是 false）
  const ifndefH5Regex = /\/\/\s*#ifndef\s+H5\s*\n([\s\S]*?)\/\/\s*#endif/gm
  result = result.replace(ifndefH5Regex, () => '')

  return result
}

// Vite 插件
export function ifdefPlugin() {
  return {
    name: 'vite-plugin-ifdef',
    enforce: 'pre' as const,
    transform(code: string, id: string) {
      if (!/\.(tsx?|jsx?)$/.test(id)) return null
      if (id.includes('node_modules')) return null

      const processed = processIfDef(code)
      if (processed === code) return null

      return {
        code: processed,
        map: null
      }
    }
  }
}

// Esbuild 插件（生产构建用）
export function ifdefEsbuildPlugin() {
  return {
    name: 'ifdef',
    setup(build: any) {
      build.onLoad({ filter: /\.(tsx?|jsx?)$/ }, async (args: any) => {
        const fs = await import('fs')
        const path = await import('path')

        if (args.path.includes('node_modules')) return null

        try {
          const code = fs.readFileSync(args.path, 'utf8')
          const processed = processIfDef(code)
          if (processed === code) return null

          return {
            contents: processed,
            loader: path.extname(args.path).slice(1) as any
          }
        } catch {
          return null
        }
      })
    }
  }
}
