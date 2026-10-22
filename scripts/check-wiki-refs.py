#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
check-wiki-refs.py —— wiki 源码引用校验门禁

用途：让「引用正确性」从依赖读者变成可脚本回归。
两种模式：
  A. 模式 A（子模块已 init）—— 校验每个 `路径:行号`：文件存在 / 行号未越界 / 目标行非空
  B. 模式 B（子模块未 init）—— 校验零精确行号 + 有「行号需校对」标注

历史：WD-29 首创 / WD-31~33 加强（kuzu.md 上曾抓出 8 类引用错误）。
模式 B 分支于 2026-10-22 加入，起因是 lmdb.md 有 11 处 `mdb.c:NNNN`
藏在行内括号里，只匹配「整段被反引号包裹」的宽松正则会漏报 —— 故 LOOSE_REF
不做任何反引号/括号假设，用最宽松模式兜底。

模式 A 用法：
  python scripts/check-wiki-refs.py docs/db-wiki/stream/kafka.md \
      --submodules stream=reference/stream/kafka --repo-root . [--show]

  --submodules <分类名>=<子模块路径>  用于把引用前缀映射到真实文件系统路径
                                    （wiki 的标准写法是「相对子模块根」）
  --show  打印每个命中行的内容，供人工做语义比对。脚本只能验「存在/越界/非空」，
          「这个行号是否真的对得上那句话」仍需人看一眼。

模式 B 用法：
  python scripts/check-wiki-refs.py --mode-b --repo-root .

退出码：0 = 全部通过；1 = 存在失败（可直接接 pre-commit / CI）
"""
import argparse
import os
import re
import sys

# 允许路径中出现的字符集（排除反引号、空格、引号等）
# 形如  `path/to/File.java:123`  或  path/to/File.scala:45,67
# 扩展名清单（长的排前面，避免 .h 抢先匹配 .hpp）
_EXTS = r'(?:java|scala|cc|cpp|cxx|hpp|h|py|proto|thrift|cmake|json|yaml|yml|sh|kt|rs|go|sql|md|txt|c|cu|cuh|lua|rb|php|s|asm)'

# 匹配 `<路径><扩展名>:<行号>`。
#关键约束：冒号后必须是行号，且冒号紧跟在扩展名之后（扩展名以字母结尾，
#因此只要要求 `:[0-9]` 即可天然排除 `file.h59` 这类误命中——那是 `.h` 后跟 `59`，
# 没有冒号）。这里不设 lookbehind，避免把 markdown 里的引用全部漏掉。
REF_RE = re.compile(
    r'(?P<path>[A-Za-z0-9_./\-]+\.' + _EXTS + r')'
    r':(?P<lines>\d+(?:[,\-]\d+)*)'
)


def parse_line_spec(spec):
    """把 '12' / '12,15' / '12-20' 展开成行号列表。"""
    out = []
    for part in spec.split(','):
        part = part.strip()
        if '-' in part:
            a, b = part.split('-', 1)
            try:
                out.extend(range(int(a), int(b) + 1))
            except ValueError:
                continue
        elif part:
            try:
                out.append(int(part))
            except ValueError:
                continue
    return out


def build_prefix_map(submodules):
    """{'stream': 'reference/stream/kafka', ...}"""
    return dict(submodules or {})


def resolve(path, prefix_map, repo_root):
    """
    解析引用路径到磁盘绝对路径。
    返回 (abs_path_or_None, matched_prefix_or_None)

    注意：必须**优先**在子模块根下查找。
    wiki 里的 `CMakeLists.txt:119` 指的是子模块内的那个（kuzu 有 448 行），
    而仓库根也有一个同名 CMakeLists.txt（14 行）。
    若先查仓库根就会解析到错误的文件，产生假失败。
    """
    # 1) 优先挂到每个子模块根下（wiki 路径的标准语义是「相对子模块根」）
    for name, sub in prefix_map.items():
        cand = os.path.join(repo_root, sub, path)
        if os.path.isfile(cand):
            return cand, name
    # 2) 再退回仓库根（学习层 / 根级文件，如 docs/... 、cmake/...）
    cand = os.path.join(repo_root, path)
    if os.path.isfile(cand):
        return cand, None
    return None, None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('files', nargs='*', help='wiki 文件（省略则只跑模式 B 合规校验）')
    ap.add_argument('--submodules', action='append', default=[],
                    help='<wiki分类名>=<子模块路径>')
    ap.add_argument('--repo-root', default='.', help='仓库根目录')
    ap.add_argument('--show', action='store_true',
                    help='打印命中的目标行内容（语义比对用）')
    ap.add_argument('--mode-b', action='store_true',
                    help='强制对全部模式 B 篇目做合规校验（零精确行号 + approx 标注）')
    ap.add_argument('--mode-readme', action='store_true',
                    help='校验 README 表格的「深化」标记与 wiki 文件实际内容一致')
    args = ap.parse_args()

    prefix_map = {}
    for s in args.submodules:
        if '=' in s:
            k, v = s.split('=', 1)
            prefix_map[k] = v

    repo_root = os.path.abspath(args.repo_root)
    total = ok = file_missing = line_oob = line_empty = 0
    failures = []

    for wf in args.files:
        if not os.path.isfile(wf):
            print(f'[SKIP] wiki 文件不存在: {wf}')
            continue
        text = open(wf, encoding='utf-8').read()
        lineno = 0
        for raw in text.splitlines():
            lineno += 1
            for m in REF_RE.finditer(raw):
                path = m.group('path')
                lines_spec = m.group('lines')
                # 跳过明显的非源码引用（本仓文档自身的行内链接等）
                if not lines_spec:
                    continue
                total += 1
                abs_path, matched = resolve(path, prefix_map, repo_root)
                if abs_path is None:
                    file_missing += 1
                    failures.append((wf, lineno, path, lines_spec, 'FILE_NOT_FOUND'))
                    continue
                content = open(abs_path, encoding='utf-8', errors='replace').read().splitlines()
                n = len(content)
                for ln in parse_line_spec(lines_spec):
                    if ln < 1 or ln > n:
                        line_oob += 1
                        failures.append((wf, lineno, path, f':{ln}', f'OUT_OF_RANGE (file has {n} lines)'))
                        continue
                    if not content[ln - 1].strip():
                        line_empty += 1
                        failures.append((wf, lineno, path, f':{ln}', 'EMPTY_LINE'))
                        continue
                    ok += 1
                    if args.show:
                        tag = matched or 'repo'
                        print(f'  [{tag}] {path}:{ln}  ->  {content[ln-1].strip()[:110]}')

    print()
    print('=' * 70)
    print(f'引用总数: {total}   通过: {ok}   '
          f'文件缺失: {file_missing}   行号越界: {line_oob}   目标行空: {line_empty}')
    if failures:
        print('-' * 70)
        for wf, wl, path, spec, reason in failures:
            print(f'  FAIL {wf}:{wl}  {path}{spec}  [{reason}]')
    print('=' * 70)
    # 返回 (退出码, 参数) —— 参数需供__main__ 决定是否跑模式 B 校验
    return (1 if failures else 0), args


# ---------------------------------------------------------------- 模式 B 合规校验
#
# 背景：模式 B 篇目（子模块未 init）禁止写精确行号，只能写 canonical 路径 +
# 「行号需校对」标注。lmdb.md 曾有11 处 `mdb.c:NNNN` 藏在行内括号里，
# 靠只匹配「整段被反引号包裹」的宽松正则才扫出来 —— 因此这里用**最宽松**的
# 「任意路径:数字」模式，不对反引号/括号做任何假设，避免同类漏报。
#
# 断言：
#   ① 模式 B 篇目零精确行号
#   ② 模式 B 篇目含「行号需校对」标注

# 任意形态的「路径:数字」——不要求反引号、不要求行尾
LOOSE_REF = re.compile(
    r'(?<![\w.])[A-Za-z_][A-Za-z0-9_./\\-]*\.(?:java|scala|kt|kts|go|rs|c|cc|cpp|cxx|h|hpp|hxx|inc'
    r'|py|pyi|pyx|cs|erl|ex|exs|clj|cljs|rb|php|js|jsx|ts|tsx|mjs|cjs|swift|m|mm'
    r'|sql|proto|thrift|graphql|gql|sh|bash|zsh|pl|lua|r|jl|ml|mli|nim|zig'
    r'|cmake|mk|gradle|bzl|bazel|toml|yml|yaml|json|xml|ini|cfg|conf'
    r'|fbs|avsc|ps1|bat|vim|el|scm|cl|pas|f90|for|vb|fs|fsx):[0-9]+'
)

# 模式 B 篇目清单（子模块未 init，禁止精确行号）
MODE_B_WIKIS = [
    'docs/db-wiki/embedded/boltdb.md',
    'docs/db-wiki/embedded/duckdb.md',
    'docs/db-wiki/embedded/lmdb.md',
    'docs/db-wiki/embedded/pklite.md',
    'docs/db-wiki/extension/arrow.md',
    'docs/db-wiki/extension/datafusion.md',
    'docs/db-wiki/extension/hnswlib.md',
    'docs/db-wiki/extension/lance.md',
    'docs/db-wiki/extension/pgvector.md',
    'docs/db-wiki/extension/velox.md',
]


def check_mode_b(repo_root):
    """校验所有模式 B 篇目：零精确行号 + 有 approx 标注。"""
    print()
    print('=' * 70)
    print('模式 B 合规校验（子模块未 init → 禁精确行号，需 approx 标注）')
    print('=' * 70)
    bad = []
    for rel in MODE_B_WIKIS:
        path = os.path.join(repo_root, rel)
        if not os.path.isfile(path):
            print(f'  [MISS] {rel}文件不存在')
            bad.append((rel, '文件不存在'))
            continue
        text = open(path, encoding='utf-8').read()
        hits = LOOSE_REF.findall(text)
        has_note = '行号需校对' in text or '未 init' in text
        status = 'OK' if (not hits and has_note) else 'FAIL'
        print(f'  [{status}] {rel.replace("docs/db-wiki/", ""):<26} '
              f'精确行号={len(hits):<3} approx标注={"有" if has_note else "无"}')
        if hits:
            bad.append((rel, f'{len(hits)} 处精确行号: {hits[:5]}'))
        if not has_note:
            bad.append((rel, '缺少「行号需校对」/「未 init」标注'))
    print('-' * 70)
    if bad:
        for rel, why in bad:
            print(f'  FAIL {rel}: {why}')
        print(f'模式 B 校验: FAIL（{len(bad)} 项不合规）')
    else:
        print(f'模式 B 校验: PASS（{len(MODE_B_WIKIS)} 篇全部合规）')
    print('=' * 70)
    return 1 if bad else 0


# ---------------------------------------------------------------- README 一致性校验
#
# 背景：README「深化」列曾把couchdb(.erl 54处) / ravendb(.cs 91 处) 误标为
# 「无行号」，把高质量档位标低了——这正是周报 P2 批评的「README 状态失真」。
# 根因是判档正则用了**固定扩展名清单**，漏掉 .erl / .cs / .ex 等冷门后缀。
# 本模式把「标记 ↔ 事实」逐篇比对固化为可回归断言，避免下次靠人手数。
#
# 断言：README 表格每行的标记 == 按文件实际内容判出的档位
#   🔬 行号级：含 `源码路径:行号`
#   🔷 canonical 标注：有源码路径但无行号
#   ◻️ 路径待核实：显式声明「不列出未经核实的源码路径」（仅符号名）

MARK_L1 = '\U0001F52C'   # 🔬
MARK_L2 = '\U0001F537'   # 🔷
MARK_L3 = '\u25FB\uFE0F'  # ◻️
NO_PATH_MARK = '不列出未经核实的源码路径'
MARK2TIER = {MARK_L1: 'L1', MARK_L2: 'L2', MARK_L3: 'L3', '\u25FB': 'L3'}


def actual_tier(path):
    """按文件实际内容判档（判据与 check_mode_b 保持一致）。"""
    text = open(path, encoding='utf-8').read()
    if LOOSE_REF.search(text):
        return 'L1'
    if NO_PATH_MARK in text:
        return 'L3'
    return 'L2'


def check_readme(repo_root):
    print()
    print('=' * 70)
    print('README 一致性校验（表格标记 ↔ wiki 文件实际内容）')
    print('=' * 70)
    readme = os.path.join(repo_root, 'docs/db-wiki/README.md')
    if not os.path.isfile(readme):
        print('  [MISS] docs/db-wiki/README.md 不存在')
        return 1

    text = open(readme, encoding='utf-8').read()
    bad = []
    total = 0
    for ln in text.split('\n'):
        if not (ln.startswith('| ') and '.md)' in ln):
            continue
        cells = [c.strip() for c in ln.strip().strip('|').split('|')]
        if len(cells) != 4:
            continue
        rel = cells[1].split('](')[-1].rstrip(')').lstrip('./')
        total += 1
        wiki = os.path.join(repo_root, 'docs/db-wiki', rel)
        if not os.path.isfile(wiki):
            bad.append((rel, '文件不存在'))
            continue
        want = actual_tier(wiki)
        got = MARK2TIER.get(cells[3])
        if want != got:
            bad.append((rel, f'标记 {cells[3]!r}，实际应为 {want}'))

    counts = {}
    for ln in text.split('\n'):
        if ln.startswith('| ') and '.md)' in ln:
            cells = [c.strip() for c in ln.strip().strip('|').split('|')]
            if len(cells) == 4:
                counts[cells[3]] = counts.get(cells[3], 0) + 1

    print(f'  逐篇比对 {total}篇')
    for mk in (MARK_L1, MARK_L2, MARK_L3):
        if mk in counts:
            print(f'    {mk} {counts[mk]} 篇')
    print('-' * 70)
    if bad:
        for rel, why in bad:
            print(f'  MISMATCH {rel}: {why}')
        print(f'README 一致性: FAIL（{len(bad)} 篇标记与事实不符）')
    else:
        print(f'README 一致性: PASS（{total} 篇标记与事实全部一致，MISMATCH 0）')
    print('=' * 70)
    return 1 if bad else 0


if __name__ == '__main__':
    rc, args = main()
    # 模式 B 校验：无文件参数时全量跑；--mode-b 强制跑；给了多个文件时也全量跑
    if (not args.files) or args.mode_b or (len(args.files) > 1):
        rc |= check_mode_b(os.path.abspath(args.repo_root))
    # 模式 README 校验：--mode-readme 读README 表格标记列，逐篇核对文件实际内容
    if args.mode_readme:
        rc |= check_readme(os.path.abspath(args.repo_root))
    sys.exit(rc)