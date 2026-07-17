/**
 * inject-grok-items.js
 *
 * 将 grok 栈条目直接注入到 items-registry.js
 * 读取 scripts/grok-entries-generated.js 中的 GROK_ENTRIES，
 * 格式化后插入到 ITEMS_REGISTRY 对象末尾
 *
 * 用法: node scripts/inject-grok-items.js
 */

const fs = require('fs');
const path = require('path');

const REGISTRY_PATH = path.resolve(__dirname, '..', 'data', 'app', 'items-registry.js');
const GROK_ENTRIES_PATH = path.join(__dirname, 'grok-entries-generated.js');

// Read generated grok entries
const grokRaw = fs.readFileSync(GROK_ENTRIES_PATH, 'utf-8');
// Extract GROK_ENTRIES array - file has `const GROK_ENTRIES = [...]`
const match = grokRaw.match(/const GROK_ENTRIES = (\[[\s\S]*\]);/);
if (!match) {
  console.error('Could not find GROK_ENTRIES array');
  process.exit(1);
}
const entries = eval('(' + match[1] + ')');

console.log(`准备注入 ${entries.length} 个 grok 条目...`);

// Read current items-registry.js
let registryContent = fs.readFileSync(REGISTRY_PATH, 'utf-8');

// Build the grok entries block
const grokBlock = entries.map(e => {
  const desc = (e.desc || '').replace(/"/g, '\\"');
  const title = (e.title || '').replace(/"/g, '\\"');
  const tags = (e.tags || []).map(t => `"${t}"`).join(', ');
  return `  "${e.key}": { product:"${e.product}", stack:"grok", quadrant:"${e.quadrant}", ring:"${e.ring}", title:"${title}", desc:"${desc}", tags:[${tags}] },`;
}).join('\n');

// Find the position before the closing `};`
const lastClosingBrace = registryContent.lastIndexOf('};');
if (lastClosingBrace === -1) {
  console.error('ERROR: Could not find closing }; in items-registry.js');
  process.exit(1);
}

// Insert grok entries before the closing };
const before = registryContent.substring(0, lastClosingBrace);
const after = registryContent.substring(lastClosingBrace);

const newContent = before + '\n\n  // ── 图解系列（xiaolincoding.com 网络+操作系统图解）──\n' + grokBlock + '\n' + after;

// Write back
fs.writeFileSync(REGISTRY_PATH, newContent, 'utf-8');
console.log(`已注入 ${entries.length} 个 grok 条目到 items-registry.js`);

// Verify
const verifyContent = fs.readFileSync(REGISTRY_PATH, 'utf-8');
const grokCount = (verifyContent.match(/stack:"grok"/g) || []).length;
console.log(`验证: 找到 ${grokCount} 个 grok 条目`);
