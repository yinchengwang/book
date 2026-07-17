"""
download_sift1m.py

下载 SIFT 1M 数据集的脚本。
官方数据源: ftp://ftp.irisa.fr/pub/texmex/corpus/

SIFT 1M 数据集包含:
- sift_base.fvecs: 1,000,000 个基准向量 (128维)
- sift_query.fvecs: 10,000 个查询向量
- sift_groundtruth.ivecs: 10,000 个查询的真值 (每个查询 top-100)
- sift_learn.fvecs: 100,000 个训练向量 (可选)

文件大小:
- sift_base.fvecs.gz: ~500MB (解压后 ~488MB)
- sift_query.fvecs.gz: ~5MB (解压后 ~5MB)
- sift_groundtruth.ivecs.gz: ~4MB (解压后 ~4MB)
"""

import urllib.request
import urllib.error
import gzip
import shutil
import os
import sys
from pathlib import Path

# 数据源配置
BASE_URL = "ftp://ftp.irisa.fr/pub/texmex/corpus/"
FILES = {
    "sift_base.fvecs.gz": "sift_base.fvecs",
    "sift_query.fvecs.gz": "sift_query.fvecs",
    "sift_groundtruth.ivecs.gz": "sift_groundtruth.ivecs",
}

def download_file(url: str, dest: Path, desc: str = None):
    """下载文件并显示进度"""
    if desc:
        print(f"下载 {desc}...")

    try:
        # 使用 urllib 下载
        urllib.request.urlretrieve(url, dest)
        print(f"  ✓ 已保存到 {dest}")
        return True
    except urllib.error.URLError as e:
        print(f"  ✗ 下载失败: {e}")
        return False
    except Exception as e:
        print(f"  ✗ 错误: {e}")
        return False

def decompress_gz(gz_path: Path, dest_path: Path):
    """解压 .gz 文件"""
    print(f"解压 {gz_path.name}...")
    try:
        with gzip.open(gz_path, 'rb') as f_in:
            with open(dest_path, 'wb') as f_out:
                shutil.copyfileobj(f_in, f_out)
        print(f"  ✓ 已解压到 {dest_path}")
        return True
    except Exception as e:
        print(f"  ✗ 解压失败: {e}")
        return False

def main():
    # 设置输出目录
    script_dir = Path(__file__).parent
    output_dir = script_dir / "sift"
    origin_dir = script_dir / "origin"

    output_dir.mkdir(exist_ok=True)
    origin_dir.mkdir(exist_ok=True)

    print("=" * 60)
    print("SIFT 1M 数据集下载工具")
    print("=" * 60)
    print(f"输出目录: {output_dir}")
    print(f"临时目录: {origin_dir}")
    print()

    # 检查是否已存在
    existing = []
    for gz_name, f_name in FILES.items():
        dest = output_dir / f_name
        if dest.exists():
            existing.append(f_name)

    if existing:
        print("已存在的文件:")
        for f in existing:
            print(f"  - {f}")
        print()

    # 下载文件
    success_count = 0
    for gz_name, f_name in FILES.items():
        dest = output_dir / f_name
        gz_dest = origin_dir / gz_name

        # 如果已解压则跳过
        if dest.exists():
            print(f"跳过 {f_name} (已存在)")
            success_count += 1
            continue

        # 下载 .gz 文件
        url = BASE_URL + gz_name
        if not download_file(url, gz_dest, f_name):
            continue

        # 解压
        if decompress_gz(gz_dest, dest):
            success_count += 1
            # 删除 .gz 文件节省空间
            gz_dest.unlink()

    print()
    print("=" * 60)
    print(f"完成: {success_count}/{len(FILES)} 个文件")

    if success_count == len(FILES):
        print("✓ 所有文件下载成功！")
        print()
        print("数据集统计:")
        print("  - sift_base.fvecs: 1,000,000 向量 x 128 维")
        print("  - sift_query.fvecs: 10,000 查询向量")
        print("  - sift_groundtruth.ivecs: 10,000 真值 (top-100)")
        print()
        print("下一步: 运行 preprocess_sift.py 转换为 C 可读格式")
    else:
        print("✗ 部分文件下载失败，请检查网络连接")
        print()
        print("备用下载方式:")
        print("  1. 手动从 http://corpus-texmex.irisa.fr/ 下载")
        print("  2. 使用代理或 VPN")
        print("  3. 从镜像源下载 (如有)")

    return 0 if success_count == len(FILES) else 1

if __name__ == "__main__":
    sys.exit(main())