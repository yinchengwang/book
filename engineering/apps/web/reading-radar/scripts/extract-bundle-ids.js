/**
 * extract-bundle-ids.js
 *
 * 解析 data/app/learn-deep-bundle.js，提取已有文章的 itemId 列表，
 * 以及每篇文章的标题（第一行 # 标题）。
 *
 * 用法: node scripts/extract-bundle-ids.js
 * 输出: stdout 打印 JSON
 */

const fs = require('fs');
const path = require('path');
const vm = require('vm');

const bundlePath = path.resolve(__dirname, '..', 'data', 'app', 'learn-deep-bundle.js');
const content = fs.readFileSync(bundlePath, 'utf-8');

const sandbox = { window: {} };
const script = new vm.Script(content);
script.runInContext(vm.createContext(sandbox));

const bundle = sandbox.window.LEARN_DEEP_CONTENT;
const entries = Object.keys(bundle).map(id => {
  const md = bundle[id];
  // 提取第一行 # 标题
  const titleMatch = md.match(/^#\s+(.+)/m);
  const title = titleMatch ? titleMatch[1].trim() : '(no title)';
  const length = md.length;
  return { id, title, length };
});

console.log(JSON.stringify({
  total: entries.length,
  items: entries
}, null, 2));
