#!/usr/bin/env python3
"""
engineering/scripts/coverage/badge.py
生成 shields.io 风格的 SVG 徽章。

用法：
    python3 badge.py <coverage.json> <output.svg>
    接受 aggregate.py 的输出，生成 SVG 徽章。
"""
import io
import json
import sys
from pathlib import Path

# 强制 UTF-8 输出（Windows console 默认 GBK）
if sys.platform == "win32":
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8")
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8")


def color_for(percent: float) -> str:
    """根据覆盖率返回颜色（绿/黄/红）。"""
    if percent >= 80:
        return "#4c1"  # bright green
    elif percent >= 60:
        return "#97ca00"  # green
    elif percent >= 40:
        return "#dfb317"  # yellow
    elif percent >= 20:
        return "#fe7d37"  # orange
    else:
        return "#e05d44"  # red


def make_badge(label: str, value: str, color: str) -> str:
    """生成 shields.io 风格 SVG 徽章。"""
    # 简单字符宽度估算（实际 shields.io 用更精确算法）
    label_w = 60 + len(label) * 6
    value_w = 60 + len(value) * 6
    total_w = label_w + value_w

    return f"""<svg xmlns="http://www.w3.org/2000/svg" width="{total_w}" height="20" role="img" aria-label="{label}: {value}">
  <linearGradient id="s" x2="0" y2="100%">
    <stop offset="0" stop-color="#bbb" stop-opacity=".1"/>
    <stop offset="1" stop-opacity=".1"/>
  </linearGradient>
  <clipPath id="r"><rect width="{total_w}" height="20" rx="3" fill="#fff"/></clipPath>
  <g clip-path="url(#r)">
    <rect width="{label_w}" fill="#555"/>
    <rect x="{label_w}" width="{value_w}" fill="{color}"/>
    <rect width="{total_w}" fill="url(#s)"/>
  </g>
  <g fill="#fff" text-anchor="middle" font-family="Verdana,Geneva,DejaVu Sans,sans-serif" text-rendering="geometricPrecision" font-size="110">
    <text aria-hidden="true" x="{label_w // 2 * 10 + 5}" y="150" fill="#010101" fill-opacity=".3" transform="scale(.1)" textLength="{len(label) * 60}">{label}</text>
    <text x="{label_w // 2 * 10 + 5}" y="140" transform="scale(.1)" fill="#fff" textLength="{len(label) * 60}">{label}</text>
    <text aria-hidden="true" x="{label_w + value_w // 2 * 10 - 5}" y="150" fill="#010101" fill-opacity=".3" transform="scale(.1)" textLength="{len(value) * 60}">{value}</text>
    <text x="{label_w + value_w // 2 * 10 - 5}" y="140" transform="scale(.1)" fill="#fff" textLength="{len(value) * 60}">{value}</text>
  </g>
</svg>"""


def main():
    if len(sys.argv) != 3:
        print("用法: badge.py <coverage.json> <output.svg>", file=sys.stderr)
        sys.exit(1)

    cov_path = Path(sys.argv[1])
    svg_path = Path(sys.argv[2])

    if not cov_path.exists():
        print(f"错误: {cov_path} 不存在", file=sys.stderr)
        sys.exit(1)

    with cov_path.open() as f:
        data = json.load(f)

    total = data.get("total", {}).get("lines", 0.0)
    value = f"{total:.1f}%"
    color = color_for(total)
    svg = make_badge("coverage", value, color)

    svg_path.write_text(svg)
    print(f"✓ 生成徽章: {svg_path} ({value}, color={color})")


if __name__ == "__main__":
    main()