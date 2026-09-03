/**
 * 调试脚本
 */

import { readFileSync } from 'fs';
import { join } from 'path';

const filePath = join(__dirname, '../../src/data/learn_deep_index.ts');
const content = readFileSync(filePath, 'utf-8');

console.log('文件长度:', content.length);
console.log('前100字符:', content.substring(0, 100));

// 测试正则
const match = content.match(/export const learnDeepList: LearnDeepSummary\[\] = \[([\s\S]*?)\];\n*$/m);
console.log('match:', match ? '成功' : '失败');
if (match) {
  console.log('数组内容长度:', match[1].length);
}