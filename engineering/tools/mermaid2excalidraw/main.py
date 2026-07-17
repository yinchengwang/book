"""
Mermaid to Excalidraw 转换工具

将 Mermaid 图表转换为 Excalidraw 手绘风格
"""

import json
import os
import sys
from pathlib import Path
from typing import Optional

import click

from generator.excalidraw import generate as generate_excalidraw
from layout.layered import layout as apply_layout
from parser.ir import DiagramIR
from parser.flowchart import parse as parse_flowchart


@click.group()
@click.version_option(version="0.1.0")
def main():
    """Mermaid to Excalidraw 转换工具

    将 Mermaid 图表转换为 Excalidraw 手绘风格 JSON
    """
    pass


@main.command("convert")
@click.argument("input", type=click.Path(exists=True))
@click.option(
    "-o", "--output",
    type=click.Path(),
    required=True,
    help="输出 Excalidraw JSON 文件路径"
)
@click.option(
    "-s", "--style",
    type=click.Path(exists=True),
    default=None,
    help="样式配置文件路径 (暂不支持)"
)
@click.option(
    "--width",
    type=int,
    default=800,
    help="画布宽度 (默认: 800)"
)
@click.option(
    "--height",
    type=int,
    default=600,
    help="画布高度 (默认: 600)"
)
@click.option(
    "--theme",
    type=click.Choice(["light", "dark"]),
    default="light",
    help="颜色主题 (默认: light)"
)
def convert(input: str, output: str, style: Optional[str], width: int, height: int, theme: str):
    """转换单个 Mermaid 文件为 Excalidraw JSON

    示例:
        mermaid2excalidraw convert diagram.mmd -o output.excalidraw.json
        mermaid2excalidraw convert diagram.mmd -o out.json --theme dark
    """
    try:
        # 读取输入文件
        with open(input, "r", encoding="utf-8") as f:
            content = f.read()

        # 解析 Mermaid
        click.echo(f"解析 Mermaid 图表: {input}")
        ir = parse_flowchart(content)

        if not ir.nodes:
            click.echo("警告: 未解析到任何节点", err=True)
            ir = DiagramIR()

        click.echo(f"  - 节点数: {len(ir.nodes)}")
        click.echo(f"  - 边数: {len(ir.edges)}")

        # 应用布局
        click.echo("应用层次布局...")
        ir = apply_layout(ir)

        # 生成 Excalidraw JSON
        click.echo("生成 Excalidraw JSON...")
        result = generate_excalidraw(ir, theme=theme)

        # 写入输出文件
        with open(output, "w", encoding="utf-8") as f:
            json.dump(result, f, indent=2, ensure_ascii=False)

        click.echo("OK] 转换完成: " + output)
        click.echo("  - 元素数: " + str(len(result.get('elements', []))))

    except Exception as e:
        click.echo(f"错误: {e}", err=True)
        sys.exit(1)


@main.command("batch")
@click.argument("input_dir", type=click.Path(exists=True, file_okay=False, dir_okay=True))
@click.option(
    "-o", "--output-dir",
    type=click.Path(file_okay=False, dir_okay=True),
    required=True,
    help="输出目录"
)
@click.option(
    "--pattern",
    default="*.mmd",
    help="文件匹配模式 (默认: *.mmd)"
)
@click.option(
    "-r", "--recursive",
    is_flag=True,
    help="递归处理子目录"
)
@click.option(
    "--theme",
    type=click.Choice(["light", "dark"]),
    default="light",
    help="颜色主题 (默认: light)"
)
def batch(input_dir: str, output_dir: str, pattern: str, recursive: bool, theme: str):
    """批量转换目录中的 Mermaid 文件

    示例:
        mermaid2excalidraw batch diagrams/ -o output/
        mermaid2excalidraw batch . -o excalidraw/ -r --theme dark
    """
    input_path = Path(input_dir)
    output_path = Path(output_dir)

    # 创建输出目录
    output_path.mkdir(parents=True, exist_ok=True)

    # 收集所有匹配的文件
    if recursive:
        files = list(input_path.rglob(pattern))
    else:
        files = list(input_path.glob(pattern))

    if not files:
        click.echo(f"未找到匹配 '{pattern}' 的文件", err=True)
        return

    click.echo(f"找到 {len(files)} 个文件")

    success_count = 0
    error_count = 0

    for file_path in files:
        try:
            # 计算相对路径
            rel_path = file_path.relative_to(input_path)
            output_file = output_path / rel_path.with_suffix(".excalidraw.json")

            # 确保输出目录存在
            output_file.parent.mkdir(parents=True, exist_ok=True)

            # 读取并解析
            with open(file_path, "r", encoding="utf-8") as f:
                content = f.read()

            ir = parse_flowchart(content)
            ir = apply_layout(ir)
            result = generate_excalidraw(ir, theme=theme)

            # 写入
            with open(output_file, "w", encoding="utf-8") as f:
                json.dump(result, f, indent=2, ensure_ascii=False)

            click.echo("[OK] " + file_path + " -> " + str(output_file))
            success_count += 1

        except Exception as e:
            click.echo("[FAIL] " + str(file_path) + ": " + str(e), err=True)
            error_count += 1

    click.echo(f"\n完成: {success_count} 成功, {error_count} 失败")


@main.command("preview")
@click.argument("input", type=click.Path(exists=True))
@click.option(
    "--theme",
    type=click.Choice(["light", "dark"]),
    default="light",
    help="颜色主题 (默认: light)"
)
def preview(input: str, theme: str):
    """预览转换结果 (输出到 stdout)

    示例:
        mermaid2excalidraw preview diagram.mmd | jq .
    """
    try:
        with open(input, "r", encoding="utf-8") as f:
            content = f.read()

        ir = parse_flowchart(content)
        ir = apply_layout(ir)
        result = generate_excalidraw(ir, theme=theme)

        click.echo(json.dumps(result, indent=2, ensure_ascii=False))

    except Exception as e:
        click.echo(f"错误: {e}", err=True)
        sys.exit(1)


# 便捷入口点
def cli():
    main()


if __name__ == "__main__":
    main()
