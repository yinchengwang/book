#!/usr/bin/env python3
"""Format MD files: headings, code fences, watermark cleanup."""
import os, re, sys

INPUT_DIR = "C:/code/book/project/reading-radar/data/interview-questions/cpp-language"

CJK_FIXES = {
    '\u2f6c': '见', '\u2f6d': '见', '\u2f6e': '见', '\u2f6f': '见',
    '\u2f70': '见', '\u2f71': '见', '\u2f72': '见', '\u2f73': '见',
    '\u2f74': '见', '\u2f75': '见', '\u2f76': '见', '\u2f77': '见',
    '\u2f78': '见', '\u2f79': '见', '\u2f7a': '见', '\u2f7b': '见',
    '\u2f7c': '见', '\u2f7d': '见', '\u2f7e': '见', '\u2f7f': '见',
    '\u2f80': '见', '\u2f81': '见', '\u2f82': '见', '\u2f83': '见',
    '\u2f84': '见', '\u2f85': '见', '\u2f86': '见', '\u2f87': '见',
    '\u2f88': '见', '\u2f89': '见', '\u2f8a': '见', '\u2f8b': '见',
    '\u2f8c': '见', '\u2f8d': '见', '\u2f8e': '见', '\u2f8f': '见',
    '\u2f90': '见', '\u2f91': '见', '\u2f92': '见', '\u2f93': '见',
    '\u2f94': '见', '\u2f95': '见', '\u2f96': '见', '\u2f97': '见',
    '\u2f98': '见', '\u2f99': '见', '\u2f9a': '见', '\u2f9b': '见',
    '\u2f9c': '见', '\u2f9d': '见', '\u2f9e': '见', '\u2f9f': '见',
    '\u2fa0': '见', '\u2fa1': '见', '\u2fa2': '见', '\u2fa3': '见',
    '\u2fa4': '见', '\u2fa5': '见', '\u2fa6': '见', '\u2fa7': '见',
    '\u2fa8': '见', '\u2fa9': '见', '\u2faa': '见', '\u2fab': '见',
    '\u2fac': '见', '\u2fad': '见', '\u2fae': '见', '\u2faf': '见',
    '\u2fb0': '见', '\u2fb1': '见', '\u2fb2': '见', '\u2fb3': '见',
    '\u2fb4': '见', '\u2fb5': '见', '\u2fb6': '见', '\u2fb7': '见',
    '\u2fb8': '见', '\u2fb9': '见', '\u2fba': '见', '\u2fbb': '见',
    '\u2fbc': '见', '\u2fbd': '见', '\u2fbe': '见', '\u2fbf': '见',
    '\u2fc0': '见', '\u2fc1': '见', '\u2fc2': '见', '\u2fc3': '见',
    '\u2fc4': '见', '\u2fc5': '见', '\u2fc6': '见', '\u2fc7': '见',
    '\u2fc8': '见', '\u2fc9': '见', '\u2fca': '见', '\u2fcb': '见',
    '\u2fcc': '见', '\u2fcd': '见', '\u2fce': '见', '\u2fcf': '见',
    '\u2fd0': '见', '\u2fd1': '见', '\u2fd2': '见', '\u2fd3': '见',
    '\u2fd4': '见', '\u2fd5': '见', '\u2fd6': '见', '\u2fd7': '见',
    '\u2fd8': '见', '\u2fd9': '见', '\u2fda': '见', '\u2fdb': '见',
    '\u2fdc': '见', '\u2fdd': '见', '\u2fde': '见', '\u2fdf': '见',
    '\u2fe0': '见', '\u2fe1': '见', '\u2fe2': '见', '\u2fe3': '见',
    '\u2fe4': '见', '\u2fe5': '见', '\u2fe6': '见', '\u2fe7': '见',
    '\u2fe8': '见', '\u2fe9': '见', '\u2fea': '见', '\u2feb': '见',
    '\u2fec': '见', '\u2fed': '见', '\u2fee': '见', '\u2fef': '见',
    '\u2ff0': '见', '\u2ff1': '见', '\u2ff2': '见', '\u2ff3': '见',
    '\u2ff4': '见', '\u2ff5': '见', '\u2ff6': '见', '\u2ff7': '见',
    '\u2ff8': '见', '\u2ff9': '见', '\u2ffa': '见', '\u2ffb': '见',
    '\u2ffc': '见', '\u2ffd': '见', '\u2ffe': '见', '\u2fff': '见',
}

CODE_PATTERNS = [
    r'#include\s+',
    r'std::',
    r'int\s+main\s*\(',
    r'void\s+\w+\s*\(',
    r'for\s*\(',
    r'\+\+',
    r'::',
    r'//\s+',
    r'if\s*\(',
    r'else\s',
    r'class\s+\w+',
    r'struct\s+\w+',
    r'template\s*<',
    r'return\s+',
    r'using\s+',
    r'new\s+',
    r'delete\s+',
    r'const\s+',
    r'auto\s+',
    r'bool\s+',
    r'cout\s*<<',
    r'cin\s*>>',
    r'endl',
]

KEYWORD_HEADINGS = [
    '精炼回答', '详细分析',
    '基本用法示例', '高级用法示例', '性能对比', '常见问题',
    '总结', '示例代码', '关键点', '实现方式', '应用场景',
    '注意事项', '底层原理', '优化策略', '典型应用', '核心区别',
    '主要优势', '主要特点', '实现原理', '使用场景', '最佳实践',
    '结构定义', '工作原理', '算法流程', '核心思想', '模板定义',
    '特化示例', '高级特性', '性能分析', '实现示例',
    '本质概念', '基本概念',
]


def fix_cjk(text):
    return "".join(CJK_FIXES.get(c, c) for c in text)


def remove_trailing_watermark(line):
    return re.sub(r'\s*[refOfO革牛：序程小]\s*$', '', line)


def is_code_line(line):
    for pat in CODE_PATTERNS:
        if re.search(pat, line):
            return True
    return False


def process_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    out = []
    i = 0
    while i < len(lines):
        raw = lines[i].rstrip('\n')
        stripped = raw.strip()

        # Skip pure watermark lines
        if re.match(r'^[refOfO革牛：序程小]+$', stripped):
            i += 1
            continue

        # Remove trailing watermark chars
        clean_stripped = remove_trailing_watermark(stripped)

        # Keyword headings: ## 精炼回答 / ## 详细分析
        if clean_stripped in KEYWORD_HEADINGS:
            out.append("## " + clean_stripped)
            i += 1
            continue

        # Numbered section headings: 1.xxx / 1.1.xxx
        parts = clean_stripped.split(None, 1)
        if len(parts) == 2:
            num_str = parts[0].rstrip('.')
            title = parts[1]
            if re.match(r'^\d+(\.\d+)*$', num_str) and not title.startswith(("：", ":", "；", ";", "，", ",", "、")) and "：" not in title and ":" not in title:
                depth = num_str.count(".") + 3
                out.append("#" * min(depth, 6) + " " + clean_stripped)
                i += 1
                continue

        # Check if this line starts a code block
        if is_code_line(stripped):
            # Collect consecutive code lines
            code_lines = [stripped]
            i += 1
            while i < len(lines):
                next_raw = lines[i].rstrip('\n')
                next_stripped = next_raw.strip()
                if is_code_line(next_stripped) or next_stripped == '' or re.match(r'^[refOfO革牛：序程小]+$', next_stripped):
                    if next_stripped and not re.match(r'^[refOfO革牛：序程小]+$', next_stripped):
                        code_lines.append(remove_trailing_watermark(next_stripped))
                    i += 1
                else:
                    break
            # Wrap in code fence
            out.append('```cpp')
            out.extend(code_lines)
            out.append('```')
            continue

        # Regular line
        out.append(stripped)
        i += 1

    # Collapse multiple blank lines
    result = []
    blank_count = 0
    for line in out:
        if line.strip() == '':
            blank_count += 1
            if blank_count <= 2:
                result.append('')
        else:
            blank_count = 0
            result.append(line)

    return '\n'.join(result).rstrip('\n') + '\n'


def main():
    files = sorted(f for f in os.listdir(INPUT_DIR) if f.endswith('.md'))
    for fname in files:
        filepath = os.path.join(INPUT_DIR, fname)
        try:
            result = process_file(filepath)
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(result)
            print(f"  OK: {fname}")
        except Exception as e:
            print(f"  ERR: {fname} - {e}", file=sys.stderr)
    print("Done")


if __name__ == '__main__':
    main()
