#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
数据集下载脚本

用于下载 ANN Benchmarks 标准数据集
"""

import argparse
import sys
from pathlib import Path

# 添加项目根目录到路径
project_root = Path(__file__).parent.parent.parent.parent
sys.path.insert(0, str(project_root))

from benchmark.core.dataset_manager import DatasetManager


def main():
    """主函数"""
    parser = argparse.ArgumentParser(
        description="下载 ANN Benchmarks 数据集",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python -m benchmark.scripts.download_datasets --all
  python -m benchmark.scripts.download_datasets --dataset glove-100-angular
  python -m benchmark.scripts.download_datasets --list
  python -m benchmark.scripts.download_datasets --dataset fashion-mnist-784-euclidean --force
        """
    )
    parser.add_argument(
        "--dataset",
        type=str,
        default="all",
        help="数据集名称 (all/名称/...)"
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="列出所有可用数据集"
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="强制重新下载"
    )
    args = parser.parse_args()

    manager = DatasetManager()

    # 列出数据集
    if args.list:
        print("Available datasets:")
        print("-" * 50)
        datasets = manager.list_datasets()
        for name in datasets:
            config = manager.config.get("datasets", {}).get(name, {})
            dim = config.get("dimension", "N/A")
            size = config.get("size", "N/A")
            distance = config.get("distance", "N/A")
            desc = config.get("description", "")
            print(f"  {name}")
            print(f"    - Dimension: {dim}")
            print(f"    - Size: {size:,}")
            print(f"    - Distance: {distance}")
            if desc:
                print(f"    - {desc}")
            print()
        return 0

    # 下载数据集
    if args.dataset == "all":
        print("[Downloader] Downloading all datasets...")
        print("-" * 50)
        datasets = manager.list_datasets()
        for name in datasets:
            try:
                manager.load(name, force_download=args.force)
                print(f"  ✓ {name}")
            except Exception as e:
                print(f"  ✗ {name}: {e}")
                return 1
    else:
        print(f"[Downloader] Downloading {args.dataset}...")
        try:
            manager.load(args.dataset, force_download=args.force)
            print(f"  ✓ {args.dataset}")
        except Exception as e:
            print(f"  ✗ {args.dataset}: {e}", file=sys.stderr)
            return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
