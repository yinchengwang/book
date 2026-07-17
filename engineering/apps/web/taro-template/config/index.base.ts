/**
 * @book/taro-template — 共享 Taro config 基础
 * 子项目应基于此扩展：
 *   const baseConfig = require('@book/taro-template/config/index.base').default;
 *   module.exports = { ...baseConfig, /* 项目特有配置 */ };
 */

import type { IConfig } from '@tarojs/taro';

const config: IConfig = {
  projectName: 'CHANGE_ME_PROJECT_NAME',
  date: '2024-01-01 00:00:00',
  designWidth: 750,
  deviceRatio: { 640: 2.34 / 2, 750: 1, 828: 1.81 / 2, 375: 2 / 1 },
  sourceRoot: 'src',
  outputRoot: 'dist',
  plugins: ['@tarojs/plugin-framework-react'],
  defineConstants: {},
  copy: { patterns: [], options: {} },
  framework: 'react',
  compilerOptions: {
    types: ['@tarojs/taro'],
    redundantRemove: true,
  },
  sass: {
    resource: [],
  },
  mini: {
    webpackChain(chain: any) {},
    postcss: {
      pxtransform: { enable: true, designWidth: 750 },
    },
  },
  h5: {
    publicPath: '/',
    staticDirectory: 'static',
    output: { filename: 'js/[name].[hash:8].js' },
    miniCssExtractPluginOption: { ignoreOrder: true, filename: 'css/[name].[hash].css' },
    postcss: { autoprefixer: { enable: true }, pxtransform: { enable: true, designWidth: 750 } },
    dllWebpackChain: {},
  },
};

export default config;
