#!/usr/bin/env python3
"""
GGUF to FP32 binary converter for MiniLM-L6 embedding model.

Reads all-MiniLM-L6-v2.Q4_K_M.gguf (or any GGUF MiniLM), extracts:
- weights.bin: all weights as FP32 (little-endian, contiguous)
- vocab.txt: wordpiece vocabulary, one token per line
- meta.json: model hyperparameters and tensor layout

Usage:
    python gguf_to_fp32.py <input.gguf> <output_dir>

Output layout (weights.bin):
    All tensor weights concatenated as FP32. Layout matches meta.json.
"""

import json
import struct
import sys
from pathlib import Path

# GGUF magic
GGUF_MAGIC = b'GGUF'

# GGML types (subset relevant to MiniLM)
# Note: GGML enum has gaps (no types 4-5, jumps from Q4_1=3 to Q5_0=6)
GGML_TYPE_F32 = 0
GGML_TYPE_F16 = 1
GGML_TYPE_Q4_0 = 2
GGML_TYPE_Q4_1 = 3
GGML_TYPE_Q5_0 = 6
GGML_TYPE_Q5_1 = 7
GGML_TYPE_Q8_0 = 8
GGML_TYPE_Q8_1 = 9
GGML_TYPE_Q2_K = 10
GGML_TYPE_Q3_K = 11
GGML_TYPE_Q4_K = 12
GGML_TYPE_Q5_K = 13
GGML_TYPE_Q6_K = 14

# Q4_K constants (llama.cpp dequantize formula)
Q4_K_BLOCK_SIZE = 256  # 256 weights per block
Q4_K_BLOCK_BYTES = 144  # bytes per block

# -----------------------------------------------------------------------------
# Q4_K dequantize (llama.cpp reference algorithm, no external deps)
# -----------------------------------------------------------------------------

def fp16_to_f32(h_bytes):
    """Convert 2-byte FP16 to float32."""
    # IEEE 754 half-precision (5 exp, 10 mantissa)
    h = struct.unpack('<H', h_bytes)[0]
    s = (h >> 15) & 0x01
    e = (h >> 10) & 0x1F
    m = h & 0x3FF
    if e == 0:
        # subnormal
        return ((-1) ** s) * (m / 1024.0) * (2 ** (-14))
    elif e == 0x1F:
        # inf/nan
        return float('inf') if m == 0 else float('nan')
    else:
        # normal
        return ((-1) ** s) * (1 + m / 1024.0) * (2 ** (e - 15))


def dequantize_q4_k(block_bytes):
    """
    Dequantize one Q4_K block (144 bytes) to 256 floats.
    Block layout:
        offset 0:   2 bytes fp16 d      (scale)
        offset 2:   2 bytes fp16 min    (min)
        offset 4:   2 bytes fp16 dmin   (min scale for sub-blocks)
        offset 6:   6 bytes padding
        offset 12:  128 bytes packed 4-bit weights (256 values)
    Returns list of 256 floats.
    """
    d = fp16_to_f32(block_bytes[0:2])
    min_val = fp16_to_f32(block_bytes[2:4])
    dmin = fp16_to_f32(block_bytes[4:6])

    weights = []
    packed = block_bytes[12:12+128]
    for byte_val in packed:
        # low nibble
        w_low = ((byte_val & 0x0F) - 8) * d + min_val
        # high nibble
        w_high = (((byte_val >> 4) & 0x0F) - 8) * d + min_val
        weights.extend([w_low, w_high])

    assert len(weights) == 256, f"Expected 256 weights, got {len(weights)}"
    return weights


# -----------------------------------------------------------------------------
# GGUF parsing (v3 format)
# -----------------------------------------------------------------------------

def read_string(data, offset):
    """Read GGUF string (uint64 length + bytes). Returns (string, next_offset)."""
    length = struct.unpack_from('<Q', data, offset)[0]
    offset += 8
    s = data[offset:offset + length].decode('utf-8', errors='replace')
    return s, offset + length


def read_metadata_value(data, offset, value_type):
    """Read one GGUF metadata value. Returns (value, next_offset)."""
    if value_type == 0:  # UINT8
        return struct.unpack_from('<B', data, offset)[0], offset + 1
    elif value_type == 1:  # INT8
        return struct.unpack_from('<b', data, offset)[0], offset + 1
    elif value_type == 2:  # UINT16
        return struct.unpack_from('<H', data, offset)[0], offset + 2
    elif value_type == 3:  # INT16
        return struct.unpack_from('<h', data, offset)[0], offset + 2
    elif value_type == 4:  # UINT32
        return struct.unpack_from('<I', data, offset)[0], offset + 4
    elif value_type == 5:  # INT32
        return struct.unpack_from('<i', data, offset)[0], offset + 4
    elif value_type == 6:  # FLOAT32
        return struct.unpack_from('<f', data, offset)[0], offset + 4
    elif value_type == 7:  # BOOL
        return bool(struct.unpack_from('<?', data, offset)[0]), offset + 1
    elif value_type == 8:  # STRING
        s, next_off = read_string(data, offset)
        return s, next_off
    elif value_type == 9:  # ARRAY
        # Read array: value_type(uint32) + length(uint64) + values
        array_elem_type = struct.unpack_from('<I', data, offset)[0]
        offset += 4
        array_len = struct.unpack_from('<Q', data, offset)[0]
        offset += 8
        values = []
        for _ in range(array_len):
            v, offset = read_metadata_value(data, offset, array_elem_type)
            values.append(v)
        return values, offset
    elif value_type == 10:  # UINT64
        return struct.unpack_from('<Q', data, offset)[0], offset + 8
    elif value_type == 11:  # INT64
        return struct.unpack_from('<q', data, offset)[0], offset + 8
    elif value_type == 12:  # FLOAT64
        return struct.unpack_from('<d', data, offset)[0], offset + 8
    else:
        raise ValueError(f"Unknown metadata value type: {value_type}")


def parse_gguf(filepath):
    """Parse GGUF file. Returns (metadata_dict, tensors_list, data_start_offset)."""
    with open(filepath, 'rb') as f:
        data = f.read()

    if data[:4] != GGUF_MAGIC:
        raise ValueError(f"Not a GGUF file: {filepath}")

    # GGUF v3 header: magic(4) + version(4) + tensor_count(uint64, 8) + kv_count(uint64, 8) = 24 bytes
    version = struct.unpack_from('<I', data, 4)[0]
    tensor_count = struct.unpack_from('<Q', data, 8)[0]
    kv_count = struct.unpack_from('<Q', data, 16)[0]
    print(f"GGUF v{version}, {tensor_count} tensors, {kv_count} kv pairs")

    offset = 24  # skip header

    # Read metadata KV pairs
    metadata = {}
    for _ in range(kv_count):
        key, offset = read_string(data, offset)
        value_type = struct.unpack_from('<I', data, offset)[0]
        offset += 4
        value, offset = read_metadata_value(data, offset, value_type)
        metadata[key] = value

    # Read tensor index entries (no alignment here; alignment is for tensor data)
    tensors = []
    for _ in range(tensor_count):
        name, offset = read_string(data, offset)
        n_dims = struct.unpack_from('<I', data, offset)[0]
        offset += 4
        # GGUF dims are uint64 array (8 bytes each)
        dims = list(struct.unpack_from(f'<{n_dims}Q', data, offset))
        offset += 8 * n_dims
        dtype = struct.unpack_from('<I', data, offset)[0]
        offset += 4
        rel_offset = struct.unpack_from('<Q', data, offset)[0]
        offset += 8
        tensors.append({
            'name': name,
            'dims': dims,
            'dtype': dtype,
            'offset': rel_offset,
        })

    # Tensor data is 32-byte aligned from tensor index end
    data_start = (offset + 31) & ~31

    return metadata, tensors, data_start, data


# -----------------------------------------------------------------------------
# Tensor extraction
# -----------------------------------------------------------------------------

def tensor_nbytes(dims, dtype):
    """Calculate tensor data size in bytes."""
    total = 1
    for d in dims:
        total *= d
    if dtype == GGML_TYPE_F32:
        return total * 4
    elif dtype == GGML_TYPE_F16:
        return total * 2
    elif dtype == GGML_TYPE_Q4_0:
        # 32 weights per 18-byte block
        return (total // 32) * 18
    elif dtype == GGML_TYPE_Q4_1:
        # 32 weights per 20-byte block
        return (total // 32) * 20
    elif dtype == GGML_TYPE_Q4_K:
        # 256 weights per 144-byte block
        return (total // 256) * 144
    elif dtype == GGML_TYPE_Q5_K:
        # 256 weights per 176-byte block
        return (total // 256) * 176
    elif dtype == GGML_TYPE_Q6_K:
        # 256 weights per 210-byte block (Q6_K uses ql + qh + scales + 16x16 int8)
        return (total // 256) * 210
    elif dtype == GGML_TYPE_Q8_0:
        # 32 weights per 34-byte block (2 bytes fp16 scale + 32 bytes int8)
        return (total // 32) * 34
    elif dtype == GGML_TYPE_Q5_0:
        # 32 weights per 22-byte block (2 bytes fp16 scale + 4 bytes qh + 16 bytes qs)
        return (total // 32) * 22
    elif dtype == GGML_TYPE_Q8_1:
        # 32 weights per 50-byte block
        return (total // 32) * 50
    else:
        raise ValueError(f"Unsupported dtype {dtype}")


def extract_tensor(data, base_offset, tensor_info):
    """Extract one tensor as list of FP32 values."""
    dims = tensor_info['dims']
    dtype = tensor_info['dtype']
    offset = base_offset + tensor_info['offset']
    nbytes = tensor_nbytes(dims, dtype)
    raw = data[offset:offset + nbytes]

    num_elems = 1
    for d in dims:
        num_elems *= d

    if dtype == GGML_TYPE_F32:
        return list(struct.unpack_from(f'<{num_elems}f', raw))
    elif dtype == GGML_TYPE_F16:
        # Convert F16 to F32
        result = []
        for i in range(num_elems):
            h_bytes = raw[i*2:(i+1)*2]
            result.append(fp16_to_f32(h_bytes))
        return result
    elif dtype == GGML_TYPE_Q4_K:
        # Dequantize Q4_K blocks
        n_blocks = num_elems // 256
        result = []
        for b in range(n_blocks):
            block = raw[b*144:(b+1)*144]
            result.extend(dequantize_q4_k(block))
        return result
    elif dtype == GGML_TYPE_Q4_0:
        # Dequantize Q4_0: 32 weights per block (18 bytes)
        # Block: 2 bytes fp16 d + 16 bytes packed 4-bit
        n_blocks = num_elems // 32
        result = []
        for b in range(n_blocks):
            block = raw[b*18:(b+1)*18]
            d = fp16_to_f32(block[0:2])
            packed = block[2:18]
            for byte_val in packed:
                w_low = ((byte_val & 0x0F) - 8) * d
                w_high = (((byte_val >> 4) & 0x0F) - 8) * d
                result.extend([w_low, w_high])
        return result
    elif dtype == GGML_TYPE_Q6_K:
        # Dequantize Q6_K: 256 weights per 210-byte block
        # Block: ql[128] + qh[64] + scales[16] (int8) + d (fp16)
        # See llama.cpp block_q6_K_to_float for exact formula
        n_blocks = num_elems // 256
        result = []
        for b in range(n_blocks):
            block = raw[b*210:(b+1)*210]
            ql = block[0:128]
            qh = block[128:192]
            scales_int8 = struct.unpack_from('<16b', block, 192)
            d = fp16_to_f32(block[208:210])
            for n in range(256):
                # low 4 bits
                ql_idx = n // 2
                if n % 2 == 0:
                    q1 = ql[ql_idx] & 0x0F
                else:
                    q1 = (ql[ql_idx] >> 4) & 0x0F
                # high 2 bits
                qh_idx = n // 4
                qh_shift = (n % 4) * 2
                q2 = (qh[qh_idx] >> qh_shift) & 0x03
                q = (q1 | (q2 << 4)) - 32  # combine to 6-bit, signed -32..31
                # scale for this sub-block (16 sub-blocks per 256 weights)
                scale_idx = n // 16
                scale = float(scales_int8[scale_idx])
                result.append(d * scale * q)
        return result
    elif dtype == GGML_TYPE_Q5_0:
        # Dequantize Q5_0: 32 weights per 22-byte block
        # Layout: 2 bytes scale (fp16) + 4 bytes qh + 16 bytes qs (NO min field)
        n_blocks = num_elems // 32
        result = []
        for b in range(n_blocks):
            block = raw[b*22:(b+1)*22]
            scale = fp16_to_f32(block[0:2])
            qh = block[2:6]      # 4 bytes = 32 high bits (1 bit per weight)
            qs = block[6:22]     # 16 bytes = 32 low 4-bit values (2 per byte)
            for i in range(32):
                low = (qs[i // 2] >> ((i % 2) * 4)) & 0x0F
                high = (qh[i // 8] >> (i % 8)) & 0x01
                q = (low | (high << 4)) - 16  # Q5_0: unsigned 0-31, subtract 16 for signed
                result.append(scale * float(q))
        return result
    elif dtype == GGML_TYPE_Q8_0:
        # Dequantize Q8_0: 32 weights per 34-byte block
        # Block layout: 2 bytes fp16 scale + 32 bytes int8 weights
        n_blocks = num_elems // 32
        result = []
        for b in range(n_blocks):
            block = raw[b*34:(b+1)*34]
            scale = fp16_to_f32(block[0:2])
            weights = struct.unpack_from('<32b', block, 2)
            result.extend([scale * w for w in weights])
        return result
    else:
        raise ValueError(f"Dequantize not implemented for dtype {dtype}")


# -----------------------------------------------------------------------------
# Main conversion
# -----------------------------------------------------------------------------

def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input.gguf> <output_dir>", file=sys.stderr)
        sys.exit(1)

    input_path = sys.argv[1]
    output_dir = Path(sys.argv[2])
    output_dir.mkdir(parents=True, exist_ok=True)

    print(f"Parsing GGUF: {input_path}")
    metadata, tensors, data_start, data = parse_gguf(input_path)

    print(f"Data starts at offset {data_start}")
    print(f"Found {len(tensors)} tensors")

    # Extract model hyperparameters
    model_meta = {
        'architecture': metadata.get('general.architecture', 'unknown'),
        'name': metadata.get('general.name', 'unknown'),
        'block_count': metadata.get('bert.block_count', 6),
        'context_length': metadata.get('bert.context_length', 512),
        'embedding_length': metadata.get('bert.embedding_length', 384),
        'feed_forward_length': metadata.get('bert.feed_forward_length', 1536),
        'attention_head_count': metadata.get('bert.attention.head_count', 12),
        'layer_norm_epsilon': metadata.get('bert.attention.layer_norm_epsilon', 1e-12),
        'pooling_type': metadata.get('bert.pooling_type', 1),
    }
    print(f"Model: {model_meta}")

    # Extract vocabulary from tokenizer.ggml.tokens
    vocab = metadata.get('tokenizer.ggml.tokens', [])
    print(f"Vocabulary size: {len(vocab)}")

    # Save vocabulary
    vocab_path = output_dir / 'vocab.txt'
    with open(vocab_path, 'w', encoding='utf-8') as f:
        for token in vocab:
            f.write(token + '\n')
    print(f"Saved vocab to {vocab_path}")

    # Extract and save all tensor weights as FP32
    tensor_layout = {}
    weights_data = bytearray()
    current_offset = 0

    for tensor_info in tensors:
        name = tensor_info['name']
        dims = tensor_info['dims']
        dtype = tensor_info['dtype']

        num_elems = 1
        for d in dims:
            num_elems *= d

        print(f"  Extracting {name}: dims={dims}, dtype={dtype}, elems={num_elems}")

        values = extract_tensor(data, data_start, tensor_info)
        assert len(values) == num_elems, f"{name}: expected {num_elems}, got {len(values)}"

        tensor_layout[name] = {
            'dims': dims,
            'offset': current_offset,
            'num_elems': num_elems,
        }

        # Append FP32 bytes
        packed = struct.pack(f'<{num_elems}f', *values)
        weights_data.extend(packed)
        current_offset += len(packed)

    # Save weights.bin
    weights_path = output_dir / 'weights.bin'
    with open(weights_path, 'wb') as f:
        f.write(weights_data)
    print(f"Saved weights.bin ({len(weights_data)} bytes = {len(weights_data)/1024/1024:.1f} MB)")

    # Save meta.json with hyperparameters and tensor layout.
    # Flatten model fields to top-level so C++ can parse without nested JSON support.
    meta_path = output_dir / 'meta.json'
    with open(meta_path, 'w') as f:
        json.dump({
            **model_meta,  # flatten: architecture, block_count, embedding_length, etc.
            'vocab_size': len(vocab),
            'tensor_layout': tensor_layout,
        }, f, indent=2)
    print(f"Saved meta.json to {meta_path}")

    print("\nDone! Next step: run C++ embedder that reads these files.")


if __name__ == '__main__':
    main()
