"""
download_sift1m_ftp.py

使用 ftplib 直接从 FTP 服务器下载 SIFT 1M 数据集。
"""

import ftplib
import gzip
import shutil
import os
import sys
from pathlib import Path
from datetime import datetime

# FTP 配置
FTP_HOST = "ftp.irisa.fr"
FTP_PATH = "/pub/texmex/corpus/"

# 文件列表
FILES = {
    "sift_base.fvecs.gz": "sift_base.fvecs",
    "sift_query.fvecs.gz": "sift_query.fvecs",
    "sift_groundtruth.ivecs.gz": "sift_groundtruth.ivecs",
}


def download_with_progress(ftp, filename: str, local_path: Path):
    """下载文件并显示进度"""
    total_size = ftp.size(filename)
    downloaded = 0
    start_time = datetime.now()

    def callback(data):
        nonlocal downloaded
        local_file.write(data)
        downloaded += len(data)
        # 显示进度
        elapsed = (datetime.now() - start_time).total_seconds()
        if elapsed > 0:
            speed = downloaded / elapsed / 1024  # KB/s
            progress = downloaded / total_size * 100
            print(f"\r  进度: {progress:.1f}% ({downloaded/1024/1024:.1f}MB / {total_size/1024/1024:.1f}MB) "
                  f"速度: {speed:.1f} KB/s", end='', flush=True)

    print(f"下载 {filename} ({total_size/1024/1024:.1f} MB)...")

    with open(local_path, 'wb') as local_file:
        ftp.retrbinary(f"RETR {filename}", callback)

    print()  # 换行
    return True


def main():
    script_dir = Path(__file__).parent
    output_dir = script_dir / "sift"
    origin_dir = script_dir / "origin"

    output_dir.mkdir(exist_ok=True)
    origin_dir.mkdir(exist_ok=True)

    print("=" * 60)
    print("SIFT 1M 数据集 FTP 下载工具")
    print("=" * 60)
    print(f"FTP 服务器: {FTP_HOST}")
    print(f"输出目录: {output_dir}")
    print()

    try:
        # 连接 FTP
        print("连接 FTP 服务器...")
        ftp = ftplib.FTP(FTP_HOST)
        ftp.login()  # 匿名登录
        ftp.cwd(FTP_PATH)
        print(f"✓ 已连接，当前目录: {ftp.pwd()}")
        print()

        # 下载文件
        success_count = 0
        for gz_name, f_name in FILES.items():
            dest = output_dir / f_name
            gz_dest = origin_dir / gz_name

            # 检查是否已存在
            if dest.exists():
                print(f"跳过 {f_name} (已存在)")
                success_count += 1
                continue

            # 下载
            try:
                if download_with_progress(ftp, gz_name, gz_dest):
                    # 解压
                    print(f"解压 {gz_name}...")
                    with gzip.open(gz_dest, 'rb') as f_in:
                        with open(dest, 'wb') as f_out:
                            shutil.copyfileobj(f_in, f_out)
                    print(f"✓ 已解压到 {dest}")

                    # 删除压缩包
                    gz_dest.unlink()
                    success_count += 1
            except Exception as e:
                print(f"\n✗ 下载失败: {e}")
                if gz_dest.exists():
                    gz_dest.unlink()
            print()

        ftp.quit()

        print("=" * 60)
        print(f"完成: {success_count}/{len(FILES)} 个文件")

        if success_count == len(FILES):
            print("✓ 所有文件下载成功！")
            print("\n数据集统计:")
            print("  - sift_base.fvecs: 1,000,000 向量 x 128 维 (~488 MB)")
            print("  - sift_query.fvecs: 10,000 查询向量 (~5 MB)")
            print("  - sift_groundtruth.ivecs: 10,000 真值 (~4 MB)")
            print("\n下一步: python preprocess_sift.py")

        return 0 if success_count == len(FILES) else 1

    except ftplib.all_errors as e:
        print(f"\n✗ FTP 错误: {e}")
        print("\n可能的解决方案:")
        print("  1. 检查网络连接")
        print("  2. 使用代理或 VPN")
        print("  3. 稍后重试 (服务器可能繁忙)")
        return 1
    except KeyboardInterrupt:
        print("\n\n用户中断")
        return 1


if __name__ == "__main__":
    sys.exit(main())
