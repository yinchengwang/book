/**
 * @book/taro-template — 共享 babel 配置
 * 子项目应 require 此文件：
 *   babel.config.js:  module.exports = require('@book/taro-template/babel.config')
 */

module.exports = {
  presets: [
    ['taro', {
      'framework': 'react',
      'ts': true
    }]
  ]
};
