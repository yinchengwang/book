const path = require('path');

module.exports = {
  projectName: 'reading-radar-2',
  date: '2024-01-01',
  framework: 'react',
  design: 'h5',
  sourceRoot: 'src',
  outputRoot: 'dist',
  plugins: [
    '@tarojs/plugin-platform-h5',
    '@tarojs/plugin-framework-react'
  ],
  h5: {
    designWidth: 750,
    deviceRatio: {
      '640': 2.34 / 2,
      '750': 1,
      '828': 1.81 / 2
    },
    devServer: {
      port: 10086,
      host: '0.0.0.0'
    },
    publicPath: '/',
    staticDirectory: 'static'
  },
  weapp: {
    appid: '',
    compile: {}
  },
  alias: {
    '@/components': path.join(__dirname, '..', 'src/components'),
    '@/pages': path.join(__dirname, '..', 'src/pages'),
    '@/hooks': path.join(__dirname, '..', 'src/hooks'),
    '@/stores': path.join(__dirname, '..', 'src/stores'),
    '@/data': path.join(__dirname, '..', 'src/data'),
    '@/utils': path.join(__dirname, '..', 'src/utils'),
    '@/styles': path.join(__dirname, '..', 'src/styles')
  }
};
