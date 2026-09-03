/**
 * cleanup-grok-keys.js
 *
 * 清理 items-registry.js 中重复的旧格式 grok key
 * 旧格式: grok-network-1_base-how_os_deal...
 * 新格式: grok-how_os_deal...
 * 删除旧格式，只保留新格式
 */

const fs = require('fs');
const path = require('path');

const REGISTRY_PATH = path.resolve(__dirname, '..', 'data', 'app', 'items-registry.js');
let content = fs.readFileSync(REGISTRY_PATH, 'utf-8');

// Remove old-style grok entries (those with network-/os- prefix)
// Match from "grok-network-..." to the closing },
let removedCount = 0;

// Pattern: "grok-(network|os)-[^\"]+": { ... stack:"grok" ... },
const oldKeyRegex = /"grok-(?:network|os)-[^"]+":\s*\{[^}]*stack:"grok"[^}]*\},?\s*/g;

const before = content;
content = content.replace(oldKeyRegex, function(match) {
  removedCount++;
  return '';
});

if (removedCount > 0) {
  fs.writeFileSync(REGISTRY_PATH, content, 'utf-8');
  console.log(`已删除 ${removedCount} 个旧格式 grok key`);

  // Verify
  const verifyContent = fs.readFileSync(REGISTRY_PATH, 'utf-8');
  const newCount = (verifyContent.match(/"grok-(?!network|os)[^"]*":\s*\{[^}]*stack:"grok"/g) || []).length;
  console.log(`剩余新格式 grok key: ${newCount}`);
} else {
  console.log('未找到旧格式 key');
}
