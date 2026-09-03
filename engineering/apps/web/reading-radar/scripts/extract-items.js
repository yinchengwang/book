/**
 * extract-items.js
 *
 * 从 data/app/items-registry.js 提取所有 300 个知识点的结构化数据，
 * 输出为可用的 JSON 格式，供 Workflow 脚本和生成阶段使用。
 *
 * 用法: node scripts/extract-items.js
 * 输出: stdout 打印 JSON（可重定向到文件）
 */

const fs = require('fs');
const path = require('path');
const vm = require('vm');

// 读取 items-registry.js
const registryPath = path.resolve(__dirname, '..', 'data', 'app', 'items-registry.js');
const content = fs.readFileSync(registryPath, 'utf-8');

// 提取 ITEMS_REGISTRY 对象字面量
const match = content.match(
  /(?:const|var|let)\s+ITEMS_REGISTRY\s*=\s*(\{[\s\S]*?\});\s*(?:\/\/|\n|$)/
);
if (!match) {
  console.error('ERROR: Could not extract ITEMS_REGISTRY from items-registry.js');
  process.exit(1);
}

// 在沙箱中求值
const sandbox = {};
const script = new vm.Script('var ITEMS_REGISTRY = ' + match[1] + ';');
script.runInContext(vm.createContext(sandbox));

const registry = sandbox.ITEMS_REGISTRY;
const keys = Object.keys(registry);

console.log(JSON.stringify({
  total: keys.length,
  items: keys.map(id => ({
    id,
    ...registry[id]
  }))
}, null, 2));
