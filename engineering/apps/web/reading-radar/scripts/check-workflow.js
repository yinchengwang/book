const fs = require('fs');
const code = fs.readFileSync('scripts/workflow-gen.js', 'utf-8');
const issues = [];

// Check BATCHES data parses as JSON
const jsonMatch = code.match(/const BATCHES = ([\s\S]*?);/);
if (jsonMatch) {
  try {
    const parsed = JSON.parse(jsonMatch[1]);
    console.log('BATCHES parsed: ' + parsed.length + ' batches');
  } catch(e) {
    issues.push('BATCHES JSON parse error: ' + e.message);
  }
}

// Check for common escaping problems
if (code.includes('String.raw')) issues.push('contains String.raw');
if (code.includes("'\\\\n'")) issues.push('double-escaped newline');

// Check that label uses correct string concat (not template literals)
const labelLine = code.match(/label: ([^\n]+)/);
if (labelLine) console.log('Label line: ' + labelLine[1].trim());

console.log('Issues: ' + (issues.length || 'none'));
issues.forEach(i => console.log('  - ' + i));

// Final line count
console.log('Total lines: ' + code.split('\n').length);
console.log('File size: ' + (code.length / 1024).toFixed(1) + ' KB');
