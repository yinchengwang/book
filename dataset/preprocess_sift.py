"""
preprocess_sift.py

将 SIFT fvecs/ivecs 格式转换为 C 语言可直接读取的二进制格式。

原格式 (fvecs):
  - 每条记录: int32(dim) + dim 个 float32
  - 每个向量 4 + 4*dim 字节

目标格式 (bin):
  - 纯 float32 数组，无维度前缀
  - 适用于 C 语言直接 mmap 或 fread

用法:
  python preprocess_sift.py --input sift/sift_base.fvecs --output sift_processed/base.bin
"""

import struct
import argparse
from pathlib import Path
import numpy as np


def read_fvecs(path: str) -> np.ndarray:
    """Read fvecs format file, return numpy array"""
    print(f"Reading {path}...")
    with open(path, 'rb') as f:
        data = f.read()

    vectors = []
    pos = 0
    count = 0
    while pos < len(data):
        # Check if we have enough data for dim + vector
        if pos + 4 > len(data):
            break
        # Read dimension
        dim = struct.unpack('i', data[pos:pos+4])[0]
        pos += 4

        # Check if we have enough data for the vector
        if pos + dim * 4 > len(data):
            break

        # Read vector data
        vec = struct.unpack('f' * dim, data[pos:pos + dim * 4])
        pos += dim * 4

        vectors.append(vec)
        count += 1

        if count % 100000 == 0:
            print(f"  {count} vectors read...", end='\r')

    print(f"  [OK] Total {count} vectors, dimension {len(vectors[0]) if vectors else 0}")
    return np.array(vectors, dtype=np.float32)


def read_ivecs(path: str) -> np.ndarray:
    """Read ivecs format file, return numpy array"""
    print(f"Reading {path}...")
    with open(path, 'rb') as f:
        data = f.read()

    vectors = []
    pos = 0
    count = 0
    while pos < len(data):
        # Check if we have enough data for count + ids
        if pos + 4 > len(data):
            break
        # Read count
        n = struct.unpack('i', data[pos:pos+4])[0]
        pos += 4

        # Check if we have enough data for the IDs
        if pos + n * 4 > len(data):
            break

        # Read ID list
        ids = struct.unpack('i' * n, data[pos:pos + n * 4])
        pos += n * 4

        vectors.append(ids)
        count += 1

    print(f"  [OK] Total {count} records, each {len(vectors[0]) if vectors else 0} IDs")
    return np.array(vectors, dtype=np.int32)


def write_binary(data: np.ndarray, path: str, desc: str = "Data"):
    """Write pure binary format"""
    print(f"Writing {path}...")
    data.tofile(path)
    print(f"  [OK] {desc}: {data.shape} ({data.nbytes / 1024 / 1024:.2f} MB)")


def write_header(path: str, count: int, dims: int, dtype: str = "float32"):
    """Write simple header file info"""
    header_path = str(path).replace('.bin', '_header.txt')
    with open(header_path, 'w') as f:
        f.write(f"count={count}\n")
        f.write(f"dims={dims}\n")
        f.write(f"dtype={dtype}\n")
    print(f"  [OK] Header: {header_path}")


def process_sift_dataset(input_dir: str, output_dir: str, prefix: str = "sift"):
    """Process entire SIFT dataset

    Args:
        input_dir: Input directory (e.g. dataset/siftsmall/fvecs)
        output_dir: Output directory (e.g. dataset/siftsmall)
        prefix: File name prefix (sift or siftsmall)
    """
    input_dir = Path(input_dir)
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    print("=" * 60)
    print("SIFT Dataset Preprocessing")
    print("=" * 60)
    print(f"Input: {input_dir}")
    print(f"Output: {output_dir}")
    print()

    # Process base
    base_path = input_dir / f"{prefix}_base.fvecs"
    if base_path.exists():
        base = read_fvecs(str(base_path))
        write_binary(base, str(output_dir / "base.bin"), "Base vectors")
        write_header(str(output_dir / "base.bin"), base.shape[0], base.shape[1])
    else:
        print(f"WARNING: Skip base (file not found: {base_path})")

    # Process query
    query_path = input_dir / f"{prefix}_query.fvecs"
    if query_path.exists():
        query = read_fvecs(str(query_path))
        write_binary(query, str(output_dir / "query.bin"), "Query vectors")
        write_header(str(output_dir / "query.bin"), query.shape[0], query.shape[1])
    else:
        print(f"WARNING: Skip query (file not found: {query_path})")

    # Process groundtruth
    gt_path = input_dir / f"{prefix}_groundtruth.ivecs"
    if gt_path.exists():
        gt = read_ivecs(str(gt_path))
        write_binary(gt, str(output_dir / "groundtruth.bin"), "Groundtruth")
        write_header(str(output_dir / "groundtruth.bin"), gt.shape[0], gt.shape[1], "int32")
    else:
        print(f"WARNING: Skip groundtruth (file not found: {gt_path})")

    print()
    print("=" * 60)
    print("Preprocessing Complete!")
    print()
    print("Output files:")
    for f in output_dir.glob("*.bin"):
        size_mb = f.stat().st_size / 1024 / 1024
        print(f"  - {f.name}: {size_mb:.2f} MB")
    print()
    print("C Language Usage:")
    print('  float *base = read_binary("base.bin", &count, &dims);')
    print('  // Layout: base[i * dims + j] is the j-th dimension of i-th vector')


def main():
    parser = argparse.ArgumentParser(description="Preprocess SIFT dataset")
    parser.add_argument("--input", required=True, help="Input directory (e.g. dataset/siftsmall/fvecs)")
    parser.add_argument("--output", required=True, help="Output directory (e.g. dataset/siftsmall)")
    parser.add_argument("--prefix", default="sift", help="File prefix (sift or siftsmall)")
    args = parser.parse_args()

    process_sift_dataset(args.input, args.output, args.prefix)


if __name__ == "__main__":
    main()
